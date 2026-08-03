#pragma once
#include <Windows.h>
#undef min                  // Windows 매크로 제거
#undef max                  // Windows 매크로 제거
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
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
void DumpCoreTopology()
{
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    std::vector<BYTE> buf(len);
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore,
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()), &len)) return;

    BYTE maxEff = 0;
    for (BYTE* p = buf.data(); p < buf.data() + len; ) {
        auto* i = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(p);
        if (i->Relationship == RelationProcessorCore) maxEff = std::max(maxEff, i->Processor.EfficiencyClass);
        p += i->Size;
    }
    int core = 0;
    for (BYTE* p = buf.data(); p < buf.data() + len; ) {
        auto* i = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(p);
        if (i->Relationship == RelationProcessorCore) {
            KAFFINITY m = i->Processor.GroupMask[0].Mask;
            bool isP = (i->Processor.EfficiencyClass == maxEff);
            // 이 물리코어가 가진 논리번호들
            std::string logicals;
            for (int b = 0; b < 64; b++)
                if (m & ((KAFFINITY)1 << b)) 
                    logicals += std::to_string(b) + " ";
			std::cout << "Core " << core++ << ": type=" << (isP ? "P" : "E") << ", eff=" << (int)i->Processor.EfficiencyClass << ", logicals=" << logicals << std::endl;
        }
        p += i->Size;
    }
}

/*
Core 0: type=P, eff=1, logicals=0 1
Core 1: type=P, eff=1, logicals=2 3
Core 2: type=P, eff=1, logicals=4 5
Core 3: type=P, eff=1, logicals=6 7
Core 4: type=P, eff=1, logicals=8 9
Core 5: type=P, eff=1, logicals=10 11
Core 6: type=E, eff=0, logicals=12
Core 7: type=E, eff=0, logicals=13
Core 8: type=E, eff=0, logicals=14
Core 9: type=E, eff=0, logicals=15
*/