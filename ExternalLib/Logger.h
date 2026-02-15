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
            if (m_running.load() && m_logger) {
                m_logger->flush();
                m_running.store(false);
            }
            if (m_logger) {
                spdlog::shutdown();
            }
        }
        void CreateSink(const std::string& logFileName);

        void LogInfo(const std::string& msg) {
            if (m_running.load()) {
                m_logger->info(msg);
            }
        }

        void LogError(const std::string& msg) {
            if (m_running.load()) {
                m_logger->info(msg);
            }
        }

        void LogWarn(const std::string& msg) {
            if (m_running.load()) {
                m_logger->info(msg);
            }
        }
    };
}
