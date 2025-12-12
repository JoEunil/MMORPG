#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

#include <CoreLib/IMessageQueue.h>
#include <CoreLib/Message.h>
#include <CoreLib/Config.h>
#include "Config.h"

namespace Cache {
    class Handler;
    class MessagePool;
    class InMemoryQueue :public Core::IMessageQueue {
        std::vector<std::thread> m_threads;
        std::queue<Core::Message*> m_sharedQueue;
        std::mutex m_mutex;
        std::condition_variable m_cv;

        std::atomic<bool> m_running = false;
        Handler* handler;
        MessagePool* messagePool;
        
        void Initialize(Handler* h, MessagePool* m) {
            handler = h;
            messagePool = m;
        }
        
        bool IsReady() {
            if (!m_running.load() || m_threads.size() != MQ_THREADPOOL_SIZE || handler == nullptr || messagePool == nullptr)
                return false;
            return true;
        }
        
        void FlushQueue() {
            while (true)
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (m_sharedQueue.empty())
                        break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

        void Start();
        void Stop();

        void ThreadFunc();
        friend class Initializer;
        
    public:
        ~InMemoryQueue() {
            Stop();
        }
        void EnqueueMessage(Core::Message* msg) override; 
    };
}
