#pragma once

#include "CacheStorage5.h"
#include "CacheFlush.h"

#include <thread>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <algorithm>
#undef min  
#undef max   
namespace Cache {
    class FlushDispatcher {
        CacheFlush* flush;
        CacheStorage5* storage_5;
        std::thread m_thread;
        std::atomic<bool> m_running = false;
        std::chrono::steady_clock::time_point m_minTime;


        void Flush(const Key5& key, Result5& res) {
            auto command = std::make_unique<FlushCommand>();
            command->stmtID = 6;
            std::vector<uint8_t> blob(sizeof(InventoryStruct));
            std::memcpy(blob.data(), &res.data, sizeof(InventoryStruct));

            command->params.push_back(key.characterID);
            command->params.emplace_back(std::move(blob));

            flush->EnqueueFlushQ(std::move(command));
        }

        void DirtyFlush(const Key5& key, Result5& res) {
            if (m_running.load() && res.lastModified + std::chrono::seconds(30) > std::chrono::steady_clock::now()) {
                m_minTime = std::min(m_minTime, res.lastModified + std::chrono::seconds(30));
                return;
            }
            Flush(key, res);
        }

        void ThreadFunc() {
            while (m_running.load()) {
                m_minTime = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                storage_5->ForEachDirty([this](const Key5& key, Result5& res) { DirtyFlush(key, res); });
                std::this_thread::sleep_until(m_minTime);
            }
            storage_5->ForEachDirty([this](auto& key, auto& res) { DirtyFlush(key, res); });
        }

        void Initialize(CacheFlush* f, CacheStorage5* s) {
            flush = f;
            storage_5 = s;
            s->SetFlushFn([this](const Key5& key, Result5& res) { Flush(key, res); });
            m_running.store(true);
            m_thread = std::thread(&FlushDispatcher::ThreadFunc, this);
        }
        bool IsReady() {
            if (!m_running.load() || flush == nullptr || storage_5 == nullptr)
                return false;
            return true;
        }
        void Stop() {
            m_running.store(false);
            if (m_thread.joinable())
                m_thread.join();
        }

        ~FlushDispatcher() {
            Stop();
        }
        friend class Initializer;
    };
}
