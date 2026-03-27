#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>

#include <BaseLib/LockFreeQueue.h>

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
		Base::LockFreeQueue<Message*, MQ_SIZE> m_sharedQueue;

		std::atomic<bool> m_running = false;

		MessageQueueHandler* handler;
        MessagePool* messagePool;
		void Initialize(MessageQueueHandler* h, MessagePool* m) {
			handler = h;
            messagePool = m;
		}
        
        bool IsReady() {
            if (handler == nullptr)
				return false;
			if (messagePool == nullptr) 
                return false;
			if(!m_running.load(std::memory_order_relaxed))
				return false;
			if (m_threads.size() != MQ_THREADPOOL_SIZE)
				return false;
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
		void EnqueueMessage(Message* msg) override;
	};
}
