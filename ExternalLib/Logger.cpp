#include "Logger.h"
#include <fstream>

namespace External {
    void Logger::CreateSink(const std::string& logFileName) {
        if (m_running.load()) {
            return; 
        }
        std::string filePath = "logs/" + logFileName + ".log";
        std::ofstream out(filePath, std::ios::out | std::ios::binary);
        const char* bom = "\xEF\xBB\xBF";
        out.write(bom, 3);
        out.close();
        // BOM은 텍스트 파일 맨 앞의 바이트 시퀀스, 인코딩을 나타내기 위함
        // 메모장에서 자동으로 인식
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, true);
        m_logger = std::make_shared<spdlog::async_logger>(
            logFileName,
            spdlog::sinks_init_list{ sink },
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block
        );
        m_logger->set_level(spdlog::level::info);
        // 출력할 로그의 최소 레벨 기준
        m_logger->flush_on(spdlog::level::warn);
#ifdef _DEBUG
        m_logger->flush_on(spdlog::level::info);
#endif
        // flush 할 로그의 레벨
        spdlog::register_logger(m_logger);
        m_running.store(true);
    }
}
