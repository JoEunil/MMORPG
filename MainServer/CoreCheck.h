#pragma once
#include <Windows.h>
#include <vector>
#include <iostream>
void Check() {
    DWORD size = 0;
    GetLogicalProcessorInformation(nullptr, &size);

    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    GetLogicalProcessorInformation(info.data(), &size);

    for (auto& entry : info) {
        if (entry.Relationship == RelationProcessorCore) {
            // entry.ProcessorMask → 이 물리 코어에 속한 논리 코어 비트마스크
            std::cout << "Physical core mask: " << entry.ProcessorMask << std::endl;
        }
    }
}

/*
실행 결과
Physical core mask: 3  (1, 2)
Physical core mask: 12 (4, 8)
Physical core mask: 48 (16, 32)
Physical core mask: 192 (64, 128)
*/