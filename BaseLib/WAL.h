#pragma once
#include <filesystem>
#include <functional> 
#include <vector> 
#include <cstring> 
#include <iterator> 
#include <cctype> 
#include <stdexcept> 
#include <type_traits>
#include <fstream>
#include <string>
#include <mutex>
#include <cstdint>
#include <array>
#include <algorithm>
#include <atomic>
#include <Windows.h>
#include <io.h>      // _chsize_s, _get_osfhandle
#include <cstdio>    // FILE*, fopen_s

namespace Base {
#pragma pack(push ,1)
	struct WALHeader
	{
		uint32_t magic;
		uint16_t type;
		uint16_t length; // payload len
		uint64_t lsn; // LSN = (seg << 32) | offset
		uint32_t crc;
	};
#pragma pack(pop)
	// [WALHeader] [Payload]

	class WAL {
		// Wrtie ahead Logging

		// truncate 규칙
		// 운영 중 truncate(recoveryLSN 기반) : 닫힌 옛 세그먼트만 삭제 → 활성 세그먼트 번호 유지 → 자동으로 안전
		// shutdown truncate : 전부 flush 후 옛 세그먼트 삭제 + 빈 세그먼트 N + 1 생성(N 재사용 금지)
		// 종료 로직에서 순서만 잘 지켜주면 됨. 

		std::string m_fileName;
		FILE* m_fp = nullptr; // fstream은 OS 핸들을 노출하지 않아 fsync 불가.

		HANDLE m_handle = INVALID_HANDLE_VALUE; // POSIX 포팅 시: fd + fsync()로 대체
		std::mutex m_syncMutex; // fsync 중 handle이 닫히는것 방지
		std::mutex m_mutex;
		uint32_t m_segment = 1;
		uint32_t m_offset = 0; // 현재 byte offset
		std::vector<uint8_t> m_writeBuf;

		std::atomic<bool> dirty = false;  // fsync 

		uint32_t LIMIT = 0;
		const uint32_t MAGIC = 0xA2DFFD2A;
		std::array<uint32_t, 256> CRC_TABLE{};
		// CRC는 체크섬 용도

		// CRC32 테이블 생성 함수
		void generate_crc_table() {
			for (uint32_t i = 0; i < 256; ++i) {
				uint32_t c = i;
				for (int j = 0; j < 8; ++j) {
					c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
				}
				CRC_TABLE[i] = c;
			}
		}

		// CRC32 계산 함수
		uint32_t calculate_crc32(const void* data, size_t len) const {
			const uint8_t* p = static_cast<const uint8_t*>(data);
			uint32_t crc = 0xFFFFFFFF;

			for (size_t i = 0; i < len; i++) {
				crc = (crc >> 8) ^ CRC_TABLE[(crc ^ p[i]) & 0xFF];
			}

			return ~crc;
		}

		// 디렉토리에서 "base.<번호>" 세그먼트를 찾아 번호순 정렬
		std::vector<std::pair<uint64_t, std::string>> FindSegments() const {
			namespace fs = std::filesystem;
			fs::path p(m_fileName);
			fs::path dir = p.has_parent_path() ? p.parent_path() : fs::path(".");
			std::string base = p.filename().string() + "."; 
			std::vector<std::pair<uint64_t, std::string>> segs;
			if (!fs::exists(dir)) 
				return segs;

			for (auto& e : fs::directory_iterator(dir)) {
				if (!e.is_regular_file()) 
					continue;
				std::string name = e.path().filename().string();
				if (name.size() <= base.size() || name.compare(0, base.size(), base) != 0) 
					continue;
				std::string suffix = name.substr(base.size());
				if (suffix.empty() || !std::all_of(suffix.begin(), suffix.end(),
					[](unsigned char c) { 
						return std::isdigit(c); 
					})) 
					continue;
				segs.emplace_back(std::stoull(suffix), e.path().string());
			}
			std::sort(segs.begin(), segs.end());
			return segs;
		}

		// seg, offset
		std::pair<uint32_t, uint32_t> Replay(const std::function<void(const WALHeader&, const uint8_t* payload)>& apply) {
			auto segs = FindSegments();
			if (segs.empty())
				return { 1, 0 };

			uint32_t lastSeg = static_cast<uint32_t>(segs.front().first);
			uint32_t validOffset = 0;

			for (auto& [num, path] : segs) {
				std::ifstream in(path, std::ios::binary);
				if (!in)
					break; // 세그먼트 파일 자체를 못 열면 이후 LSN 순서 보장 불가

				std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
					std::istreambuf_iterator<char>());
				size_t pos = 0;
				while (pos + sizeof(WALHeader) <= buf.size()) {
					WALHeader h;
					std::memcpy(&h, buf.data() + pos, sizeof(WALHeader));

					if (h.magic != MAGIC)
						break;                                    // 깨짐 
					size_t recordSize = sizeof(WALHeader) + h.length;
					if (pos + recordSize > buf.size())
						break;                                    // 길이가 파일 밖 -> 쓰다 만 레코드

					// CRC 검증: 레코드 사본에서 crc 필드만 0으로 만들고 전체 재계산
					m_writeBuf.assign(buf.begin() + pos, buf.begin() + pos + recordSize);
					reinterpret_cast<WALHeader*>(m_writeBuf.data())->crc = 0;
					if (calculate_crc32(m_writeBuf.data(), recordSize) != h.crc)
						break;                                    // 체크섬 불일치 

					apply(h, buf.data() + pos + sizeof(WALHeader));
					pos += recordSize;
				}

				lastSeg = static_cast<uint32_t>(num);
				validOffset = static_cast<uint32_t>(pos);

				if (pos < buf.size())
					break; // 중간에 끊김 — 뒤 세그먼트가 있어도 순서 보장이 깨지므로 무시
			}
			return { lastSeg, validOffset };
		}

		bool OpenSegment(uint32_t segment)
		{
			std::string path = m_fileName + "." + std::to_string(segment);

			FILE* fp = nullptr;
			if (fopen_s(&fp, path.c_str(), "ab+") != 0)
				return false;

			int fd = _fileno(fp);
			HANDLE h = (HANDLE)_get_osfhandle(fd);
			if (h == INVALID_HANDLE_VALUE)
			{
				fclose(fp);
				return false;
			}
			{
				std::lock_guard<std::mutex> lock(m_syncMutex);
				m_fp = fp;
				m_handle = h;
			}
			m_segment = segment;
			return true;
		}

		bool RotateSegment()
		{
			if (!m_fp)
				return OpenSegment(m_segment);

			fflush(m_fp);

			{
				std::lock_guard<std::mutex> lock(m_syncMutex);
				FlushFileBuffers(m_handle);
				fclose(m_fp);
				m_fp = nullptr;
				m_handle = INVALID_HANDLE_VALUE;
			}


			uint32_t nextSegment = m_segment + 1;

			if (!OpenSegment(nextSegment))
				return false;

			m_offset = 0;
			return true;
		}

	public:
		WAL() = delete;
		WAL(std::string f, uint32_t limit, const std::function<void (const WALHeader&, const uint8_t* payload)>& a) : m_fileName(f), LIMIT(limit)
		{
			generate_crc_table();
			auto [seg, offset] = Replay(a);
			m_segment = seg;
			m_offset = offset;
			std::string path = m_fileName + "." + std::to_string(m_segment);

			// 깨진 부분 잘라내기, 실제 파일 end와 offset 위치 비교

			// std::filesystem 멤버는 실패시 throw
			if (std::filesystem::exists(path) && std::filesystem::file_size(path) > m_offset)
				std::filesystem::resize_file(path, m_offset);

			if (!OpenSegment(seg))
				throw std::runtime_error("open failed");
			m_writeBuf.reserve(1000);
		}
		~WAL() {
			std::lock_guard<std::mutex> lock(m_mutex);
			std::lock_guard<std::mutex> lock2(m_syncMutex);
			if (m_fp) {
				fflush(m_fp);              // CRT 버퍼 → OS
				FlushFileBuffers(m_handle); // OS → 디스크 (마지막 records까지 durable하게)
				fclose(m_fp);
				m_fp = nullptr;
			}
		}
		void Fsync() {
			if (!dirty.load(std::memory_order_acquire))
				return;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (!m_fp)
					return;
				fflush(m_fp);
				dirty.store(false, std::memory_order_release);
			}
			{
				std::lock_guard<std::mutex> lock(m_syncMutex);
				FlushFileBuffers(m_handle); // fsync
			}
		}

		// payload는 ( key + result ) 형태로 만들어서 넘기기
		uint64_t Write(const uint8_t* payload, size_t len, uint16_t type) {
			if (len > UINT16_MAX - sizeof(WALHeader))
				return 0;
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_fp) {
				if (!OpenSegment(m_segment))
					return 0;
			}
			m_writeBuf.resize(sizeof(WALHeader) + len);
			size_t recordSize = sizeof(WALHeader) + len;
			if (m_offset + recordSize > LIMIT) {
				if (!RotateSegment())
					return 0;
			}

			WALHeader* header = reinterpret_cast<WALHeader*>(m_writeBuf.data());
			header->magic = MAGIC;
			header->type = type;
			header->length = static_cast<uint16_t>(len);
			header->lsn = (static_cast<uint64_t>(m_segment) << 32) | m_offset;
			header->crc = 0;   // crc 필드는 0으로 crc 값 계산, crc 검증 시 crc 필드는 0으로 세팅.

			std::memcpy(m_writeBuf.data() + sizeof(WALHeader), payload, len);
			header->crc = calculate_crc32(m_writeBuf.data(), recordSize);

			// wal 기록 실패는 durability를 더이상 보장할 수 없는 상황 -> 추가 작업 block 
			// seg가 1부터 시작이기 때문에 0을 Invalid lsn으로. 
			if (fwrite(m_writeBuf.data(), recordSize, 1, m_fp) != 1) {
				fflush(m_fp); // CRT 버퍼 flush (CRT 버퍼 -> OS 페이지 캐시)
				_chsize_s(_fileno(m_fp), m_offset);   // OS 레벨 파일 자르기
				clearerr(m_fp);
				return 0;
			}

			m_offset += recordSize;
			dirty.store(true, std::memory_order_release);
			return header->lsn; 
		}

		void TruncateBefore(uint32_t boundary) {
			uint32_t limit;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				limit = (boundary < m_segment) ? boundary : m_segment;
			}
			for (auto& [num, path] : FindSegments()) {
				if (num < limit) {
					std::error_code ec;
					std::filesystem::remove(path, ec); // 실패해도 무시 — 다음 truncate가 재시도
				}
			}
		}
	};
}