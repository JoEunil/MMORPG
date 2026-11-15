#pragma once

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

#include <BaseLib/RingQueue.h>
#include "IMessageQueue.h"
#include "MessagePool.h"
#include "Message.h"
#include "LoggerGlobal.h"
#include "Config.h"

namespace Core {
	class MessageQueueHandler;
	// 수신 큐
	class InMemoryQueue :public IMessageQueue{
		std::vector<std::thread> m_threads;
		Base::RingQueue<CoreMessagePool::ScopedMessage, MQ_SIZE> m_sharedQueue;
		std::mutex m_queueMutex;
		std::condition_variable m_workAvailable;

		std::atomic<bool> m_running = false;

		MessageQueueHandler* handler = nullptr;
        CoreMessagePool* messagePool = nullptr;
		void Initialize(MessageQueueHandler* h, CoreMessagePool* m) {
			handler = h;
            messagePool = m;
		}
        
        bool IsReady() {
            if (handler == nullptr) {
                sysLogger->LogError("core mq", "handler not initialized");
                return false;
            }
            if (messagePool == nullptr) {
                sysLogger->LogError("core mq", "messagePool not initialized");
                return false;
            }
            if (!m_running.load(std::memory_order_relaxed)) {
                sysLogger->LogError("core mq", "not running");
                return false;
            }
            if (m_threads.size() != MQ_THREADPOOL_SIZE) {
                sysLogger->LogError("core mq", "invalid worker count",
                    "expected", MQ_THREADPOOL_SIZE, "actual", m_threads.size());
                return false;
            }
            return true;
        }

		void Start();
		void Stop();

		void ThreadFunc();
		friend class Initializer;
	public:
		~InMemoryQueue() {
			Stop();
		}
		bool EnqueueMessage(Message* msg) override;
	};
}
