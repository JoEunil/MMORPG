#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <atomic>
#include <thread>

#include <CoreLib/ILogger.h>
#include "JsonUtility.h"
#include "Config.h"

namespace External {
    class Logger : public Core::ILogger
    {
        std::shared_ptr<spdlog::logger> m_logger;
        std::atomic<bool> m_running = false;
    public:
        static void Initialize() {
            spdlog::init_thread_pool(LOG_Q_SIZE, LOG_THREAD_SIZE);
        }
        ~Logger() {
            if (m_running.load(std::memory_order_relaxed) && m_logger) {
                m_logger->flush();
                m_running.store(false, std::memory_order_relaxed);
            }
            // 전역 spdlog 스레드풀은 모든 로거가 공유하므로 여기서 내리지 않는다.
            // 인스턴스마다 spdlog::shutdown()을 부르면 첫 로거 파괴 시 풀이 사라져
            // 나머지 로거의 flush가 "thread pool doesn't exist" 에러를 낸다.
            // 풀 종료는 main에서 모든 로거 reset 후 한 번만 spdlog::shutdown()으로 수행.
        }
        void CreateSink(const std::string& logFileName);

        void LogInfo(const std::string& msg) override {
            if (m_running.load(std::memory_order_relaxed)) {
                m_logger->info(msg);
            }
        }

        void LogError(const std::string& msg) override {
            if (m_running.load(std::memory_order_relaxed)) {
                m_logger->info(msg);
            }
        }

        void LogWarn(const std::string& msg) override {
            if (m_running.load(std::memory_order_relaxed)) {
                m_logger->info(msg);
            }
        }
        void Flush() override {
            if (m_running.load(std::memory_order_relaxed))  {
                m_logger->flush();
            }
        }
    };
}
