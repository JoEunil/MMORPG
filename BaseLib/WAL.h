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

namespace Base {

	template <typename T, uint64_t LIMIT>
	class WAL {
		// Wrtie ahead Logging
		static_assert(std::is_trivially_copyable_v<T>,
			"WAL record must be trivially copyable (POD)");

		std::string m_fileName;
		std::ofstream m_out;
		std::mutex m_mutex;
		uint32_t m_segment = 1;
		uint64_t m_counter = 0;
		const uint32_t MAGIC = 0xA2DFFD2A;
		std::array<uint32_t, 256> CRC_TABLE{};
		// [MAGIC 4B][payload sizeof(T)][CRC32 4B]
		const size_t RECORD_SIZE = sizeof(uint32_t) + sizeof(T) + sizeof(uint32_t);
		// 복구 시 깨진 부분 처리하기 위함
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
		uint32_t calculate_crc32(const void* data, size_t len) {
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

		void Replay(const std::function<void(const T&)>& apply) {
			auto segs = FindSegments();
			if (segs.empty())
			{ 
				m_segment = 1;
				m_counter = 0; 
				return; 
			}

			for (auto& [num, path] : segs) {
				std::ifstream in(path, std::ios::binary);
				if (!in) continue;
				std::vector<char> buf((std::istreambuf_iterator<char>(in)),
					std::istreambuf_iterator<char>());
				size_t   pos = 0;
				uint64_t validCnt = 0;

				while (pos + RECORD_SIZE <= buf.size()) {
					uint32_t magic;
					std::memcpy(&magic, buf.data() + pos, sizeof(magic));
					if (magic != MAGIC) { 
						++pos; 
						continue; 
					}          // 경계 재동기화

					const char* payload = buf.data() + pos + sizeof(uint32_t);
					uint32_t storedCrc;
					std::memcpy(&storedCrc, payload + sizeof(T), sizeof(storedCrc));

					if (calculate_crc32(payload, sizeof(T)) != storedCrc) {
						++pos; 
						continue;                              // 체크섬 불일치 → 계속 스캔
					}

					T rec;
					std::memcpy(&rec, payload, sizeof(T));
					apply(rec);                                       // 멱등 적용
					pos += RECORD_SIZE;
					++validCnt;
				}
				// 마지막 세그먼트 상태를 이어감 (append 대상)
				m_segment = static_cast<uint32_t>(num);
				m_counter = validCnt;
			}
		}

	public:
		WAL() = delete;
		WAL(std::string f, const std::function<void(const T&)>& a) : m_fileName(f)
		{
			generate_crc_table();
			Replay(a);
			m_out.open(m_fileName + "." + std::to_string(m_segment), std::ios::binary | std::ios::app); 
			if (!m_out.is_open())
				throw std::runtime_error("WAL open failed: " + m_fileName);
		}
		bool Write(T& rec) {
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_counter >= LIMIT) {
				m_segment++;
				m_out.close();
				m_out.open(m_fileName + "." + std::to_string(m_segment), std::ios::binary | std::ios::app);
				m_counter = 0;
			}
			uint32_t crc = calculate_crc32(&rec, sizeof(rec));
			m_out.write(reinterpret_cast<const char*>(&MAGIC), sizeof(MAGIC));
			m_out.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
			m_out.write(reinterpret_cast<const char*>(&crc), sizeof(crc));

			m_out.flush();
			m_counter++;
			return static_cast<bool>(m_out); // 성공했는지, 스트림이 bool() operator 가지고 있음
		}
	};
}