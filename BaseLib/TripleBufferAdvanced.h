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
		std::atomic<uint16_t> flag = 0;
		// 상위 2비트는 상태 표시 , 이전 비트는 counter
		// 첫번째 비트: back1, back2 lock
		// 두번째 비트 : back2가 최신 상태인지, (write 시 1, back1-back2 swap 시 0)
		// 하위 비트: read counte
		// 두번째 비트만 1 일 때 back1-back2 swap

	public:
		void Init(T* b1, T* b2) {
			back1 = b1;
			back2 = b2;
		}
		void Write(T* write) {
			while (true)
			{
				uint16_t old_flag = flag.load(std::memory_order_relaxed);
				uint16_t flagNotUsing = old_flag & (0x8000-1); // mask: 001111...
				uint16_t flagUsing = old_flag | 0x8000;
				if (flag.compare_exchange_weak(flagNotUsing, flagUsing, std::memory_order_acq_rel, std::memory_order_relaxed))
					break;
			}
			// back1 swap 권한 획득, flag lock

			std::swap(back1, write);

			// back1 swap 종료, flag release 
			while (true)
			{
				uint16_t old_flag = flag.load(std::memory_order_relaxed);
				uint16_t flagNotUsing = old_flag & (0x4000 - 1);
				if (flag.compare_exchange_weak(old_flag, flagNotUsing, std::memory_order_acq_rel, std::memory_order_relaxed))
					break;
			}
		}

		BufferReader<T> Read() {
			T* read_ptr = nullptr;

			uint16_t new_flag;
			while (true) {
				uint16_t old_flag = flag.load(std::memory_order_relaxed);
				if (old_flag == 0) { // 최신 + not reading
					new_flag = 0x8000;          // swap 진행
				}
				else if (old_flag & 0x8000) { 
					// write 일 때에도 block 되지만, back swap일 때 새로운 read block 하기 위함.
					continue;
				}
				else {
					new_flag = old_flag + 1;  // 단순 reader count 증가
				}

				if (flag.compare_exchange_weak(old_flag, new_flag, std::memory_order_acq_rel, std::memory_order_relaxed)) {
					break; // 성공하면 루프 탈출
				}
				// CAS 실패하면 old_flag가 갱신됨 → 다시 계산
			}

			if (new_flag == 0x8000) {
				std::swap(back1, back2);
				flag.store(0x4000 + 1, std::memory_order_release);
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