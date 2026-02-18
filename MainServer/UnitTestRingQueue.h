#include <iostream>
#include <cassert>
#include <BaseLib/RingQueue.h>

void TestRingQ() {
    {
        std::cout << "Test 1: Initial state\n";
        Base::RingQueue<int, 8> q;
        assert(q.empty());
        assert(!q.full());
        assert(q.size() == 0);
        std::cout << "OK\n";
    }

    {
        std::cout << "Test 2: push test\n";
        Base::RingQueue<int, 8> q;
        q.push(10);
        assert(!q.empty());
        assert(q.size() == 1);

        q.push(20);
        assert(q.size() == 2);
        std::cout << "OK\n";
    }

    {
        std::cout << "Test 3: pop test\n";
        Base::RingQueue<int, 8> q;
        q.push(1);
        q.push(2);
        q.push(3);

        assert(q.pop() == 1);
        assert(q.pop() == 2);
        assert(q.pop() == 3);
        assert(q.empty());
        std::cout << "OK\n";
    }

    {
        std::cout << "Test 4: wrap-around \n";
        Base::RingQueue<int, 8> q;

        q.push(1);
        q.push(2);
        q.push(3);

        assert(q.pop() == 1);
        assert(q.pop() == 2);

        q.push(4);
        q.push(5);
        q.push(6);
        q.push(7); // wrap-around 발생

        // 현재 q 내용: [3,4,5,6,7]

        assert(q.pop() == 3);
        assert(q.pop() == 4);
        assert(q.pop() == 5);
        assert(q.pop() == 6);
        assert(q.pop() == 7);
        assert(q.empty());

        std::cout << "OK\n";
    }

    {
        std::cout << "Test 5: full state test\n";

        Base::RingQueue<int, 8> q;

        // SIZE=8 → max 7개 사용 가능
        for (int i = 0; i < 7; i++)
            q.push(i);

        assert(q.full());
        assert(q.size() == 7);

        std::cout << "OK\n";
    }

    std::cout << "\n모든 테스트 통과!\n";
}
