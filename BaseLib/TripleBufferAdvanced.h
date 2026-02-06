#pragma once
#include <atomic>
#include <algorithm>
#include <memory>

namespace Base {
	template <typename T>
	struct BufferReader;
		
	template <typename T>
	class TripleBufferAdvanced {
		T* back1; 
		T* back2; // reader 가 참조
		std::atomic<uint8_t> flag = 0;
		// 상위 1비트는 상태 표시 , 이전 비트는 counter
		// 최상위 비트 1: back1 swap 중
		// 최상위 비트 0: back1 사용 가능
		// 하위 7비트: read counter, back2 사용 가능 여부.

	public:
		void Init(T* b1, T* b2) {
			back1 = b1;
			back2 = b2;
		}
		void Write(T* write) {
			// (flag & 128)
			while (true)
			{
				uint8_t old_flag = flag.load(std::memory_order_relaxed);
				uint8_t flagNotUsing = old_flag & 127;
				uint8_t flagUsing = old_flag | 128;
				if (flag.compare_exchange_weak(flagNotUsing, flagUsing, std::memory_order_acq_rel, std::memory_order_relaxed))
					break;
			}
			// back1 swap 권한 획득, flag lock

			std::swap(back1, write);

			// back1 swap 종료, flag release 
			while (true)
			{
				uint8_t old_flag = flag.load(std::memory_order_relaxed);
				uint8_t flagNotUsing = old_flag & 127;
				uint8_t flagUsing = old_flag | 128;
				if (flag.compare_exchange_weak(flagUsing, flagNotUsing, std::memory_order_acq_rel, std::memory_order_relaxed))
					break;
			}
		}

		BufferReader<T> Read() {
			T* read_ptr = nullptr;

			uint8_t new_flag;
			while (true) {
				uint8_t old_flag = flag.load(std::memory_order_relaxed);
				if ((old_flag & 0x80) == 0) { // 최신 + not reading
					new_flag = 0x80;          // swap 진행
				}
				else {
					new_flag = old_flag + 1;  // 단순 reader count 증가
				}

				if (flag.compare_exchange_weak(old_flag, new_flag, std::memory_order_acq_rel, std::memory_order_relaxed)) {
					break; // 성공하면 루프 탈출
				}
				// CAS 실패하면 old_flag가 갱신됨 → 다시 계산
			}

			if (new_flag == 0x80) {
				std::swap(back1, back2);
				flag.store(1, std::memory_order_release); // swap 끝
			}

			read_ptr = back2;
			return BufferReader<T>(back2, this);
		}
		void ReadDone() {
			flag.fetch_sub(1, std::memory_order_relaxed);
		}
	};


	template <typename T>
	struct BufferReader {
		// RAII로 ReadDone 메서드 호출을 강제하기 위함.
		TripleBufferAdvanced<T>* owner;
		T* data;
		BufferReader(T* ptr, TripleBufferAdvanced<T>* o) {
			data = ptr;
			owner = o;
		}
		~BufferReader() {
			if(owner != nullptr)
				owner->ReadDone();
		}
		// 복사 금지
		BufferReader(const BufferReader&) = delete;
		BufferReader& operator=(const BufferReader&) = delete;

		// 이동 허용 
		BufferReader(BufferReader&& other) noexcept
			: owner(other.owner), data(other.data) {
			other.owner = nullptr;
			other.data = nullptr;
		}

		BufferReader& operator=(BufferReader&& other) noexcept {
			if (this != &other) {
				owner = other.owner;
				data = other.data;
				other.owner = nullptr;
				other.data = nullptr;
			}
			return *this;
		}
	};
}