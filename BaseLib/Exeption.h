#pragma once
#include <cassert>
#include <cstdlib>

namespace Base {
    [[noreturn]] inline void unreachable() {
        assert(false && "Unreachable");
        std::abort();
        // SIGABRT 시그널 발생
        // 즉시 프로세스 종료
        //OS 설정에 따라 core dump 생성
    }
}

// Core Dump에서 함수이름, 호출 스택, 실패 지점을 확인할 수 있다.
// 절대 일어나서는 안되는 경우가 발생할 때 호출하여 서버를 종료 시킨다. 
// Baselib에는 로그로 남길 상태값이 없다.