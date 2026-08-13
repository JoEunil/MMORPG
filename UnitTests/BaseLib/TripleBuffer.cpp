#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <BaseLib/TripleBuffer.h>

struct Dummy {
	int a = 0;
	int b = 0;
};
class TripleBufferTest : public ::testing::Test {
protected:
	Base::TripleBuffer<Dummy> tb;
	Dummy* writer = new Dummy();

	~TripleBufferTest() {
		delete writer;
	}
	void Initialize() {
		Dummy* back1 = new Dummy();
		Dummy* back2 = new Dummy();
		tb.Init(back1, back2);
	}
	
	void Write(int a, int b) {
		writer->a = a;
		writer->b = b;
		tb.Write(writer);
	}
};

TEST_F(TripleBufferTest, InitStatus) {
	Initialize();
	
	Base::BufferReader<Dummy> reader = tb.Read();
	
	EXPECT_EQ(reader.data->a, 0);
	EXPECT_EQ(reader.data->b, 0);
}

TEST_F(TripleBufferTest, CheckPointerSwap) {
	Initialize();

	// 초기 writer pointer 기록
	Dummy* InitialWriter = writer;
	Write(1, 2);

	Base::BufferReader<Dummy> reader = tb.Read();
	// swap 이후 reader가 초기 writer pointer를 가리키는지
	EXPECT_EQ(InitialWriter, reader.data);
	EXPECT_EQ(1, reader.data->a);
	EXPECT_EQ(2, reader.data->b);
	// swap 이후 writer pointer가 바뀌었는지
	EXPECT_NE(InitialWriter, writer); 
}

TEST_F(TripleBufferTest, WriteWhileReading) {
	Initialize();
	{
		Write(1, 2);
		Base::BufferReader<Dummy> reader1 = tb.Read();
		Write(5, 6);
		Write(5, 6);
		Write(5, 6);
		Write(3, 4);
		Base::BufferReader<Dummy> reader2 = tb.Read();
		EXPECT_EQ(reader2.data->a, 1);
		EXPECT_EQ(reader2.data->b, 2);
	}

	// reader 모두 반납 이후 최신 데이터 읽기
	Base::BufferReader<Dummy> reader3 = tb.Read();
	EXPECT_EQ(reader3.data->a, 3);
	EXPECT_EQ(reader3.data->b, 4);
}

TEST_F(TripleBufferTest, ReadFreshData) {
	Initialize();
	Write(1, 2);
	Write(3, 4);
	Write(5, 6);
	Write(7, 8);
	Write(9, 10);
	Base::BufferReader<Dummy> reader2 = tb.Read();
	EXPECT_EQ(reader2.data->a, 9);
	EXPECT_EQ(reader2.data->b, 10);
}

TEST_F(TripleBufferTest, ReaderRace) {
	Initialize();

	std::atomic<bool> stopSignal = false;

	std::vector<std::thread> readers;
	for (int i = 0; i < 3; i++) {
		readers.push_back(std::thread([&]() {
			int before = -1;
			while (!stopSignal.load(std::memory_order_relaxed)) {
				Base::BufferReader<Dummy> reader = tb.Read();
				// 단조 읽기(monotonic read) 확인 — reader가 읽은 값이 이전 값보다 작으면 안 됨
				// (eventual consistency만으로는 역행을 막지 못하므로 별도 성질)
				EXPECT_TRUE(reader.data->a >= before);
				before = reader.data->a;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			})
		);
	}
	int writerCnt = 10000000;
	std::thread writer([&]() {
		int i = 0;
		while (i < writerCnt) {
			Write(i, i);
			i++;
		}
		stopSignal.store(true, std::memory_order_relaxed);
		});

	writer.join();
	for (auto& reader : readers) {
		reader.join();
	}

	{ // 마지막 write 반영 확인
		Base::BufferReader<Dummy> reader = tb.Read();
		EXPECT_EQ(reader.data->a, writerCnt - 1);
	}

	// counter reset 확인
	Write(1, 2);

	Base::BufferReader<Dummy> reader = tb.Read();
	EXPECT_EQ(1, reader.data->a);
	EXPECT_EQ(2, reader.data->b);

}
TEST_F(TripleBufferTest, WriterReaderRace) {
	Initialize();

	std::atomic<bool> stopSignal = false;

	std::vector<std::thread> readers;
	std::vector<std::thread> writers;
	int writerCnt = 2;
	int works = 10000000;

	for (int i = 0; i < 3; i++) {
		readers.push_back(std::thread([&]() {
			int before = -1;
			while (!stopSignal.load(std::memory_order_relaxed)) {
				Base::BufferReader<Dummy> reader = tb.Read();
				// reader가 읽은 값이 유효한 값인지만 체크, tearing 발생하는지
				EXPECT_TRUE(reader.data->a < works);
				EXPECT_TRUE(reader.data->a >= 0);
				EXPECT_EQ(reader.data->a, reader.data->b);
				before = reader.data->a;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			})
		);
	}

	for (int i = 0; i < writerCnt; i++) {
		writers.push_back(std::thread([&]() {
			Dummy* local = new Dummy();
			int localCounter = 0;
			while (localCounter < works) {
				local->a = localCounter;
				local->b = localCounter++;
				tb.Write(local);
			}
			stopSignal.store(true, std::memory_order_relaxed);
			}));
	}

	for (auto& writer : writers) {
		writer.join();
	}

	for (auto& reader : readers) {
		reader.join();
	}

	// counter reset 확인
	Write(1, 2);

	Base::BufferReader<Dummy> reader = tb.Read();
	EXPECT_EQ(1, reader.data->a);
	EXPECT_EQ(2, reader.data->b);

}