#include "InMemoryQueue.h"
#include "Handler.h"
#include "MessagePool.h"

namespace Cache {
    namespace {
        Base::Priority GetMessagePriority(Core::Message* message) {
            if (message == nullptr || message->GetLength() < sizeof(Core::MsgHeader))
                return Base::Priority::Droppable;

            switch (Core::parseMsgHeader(message->GetBuffer())->messageType)
            {
            case Core::MSG_CHARACTER_LIST_REQ:
            case Core::MSG_CHARACTER_STATE_REQ:
            case Core::MSG_INVENTORY_REQ:
            case Core::MSG_CURRENCY_REQ:
            case Core::MSG_DIAMOND_REQ:
            case Core::MSG_BAZAAR_MY_LIST:
            case Core::MSG_BAZAAR_SEARCH:
                return Base::Priority::Droppable;
            case Core::MSG_CHARACTER_STATE_UPDATE:
            case Core::MSG_INVENTORY_UPDATE:
            case Core::MSG_CURRENCY_DEPOSIT:
            case Core::MSG_DIAMOND_DEPOSIT:
            case Core::MSG_BAZAAR_REGISTER:
            case Core::MSG_BAZAAR_CANCEL:
            case Core::MSG_BAZAAR_BUY:
            case Core::MSG_BAZAAR_CLAIM:
            case Core::MSG_BAZAAR_CHECK_OUTBOX:
            case Core::MSG_PROFILE_RENAME:
                return Base::Priority::Important;
            default:
                Core::errorLogger->LogWarn(
                    "cache mq","unknown message priority", "messageType", Core::parseMsgHeader(message->GetBuffer())->messageType);
                return Base::Priority::Important;
            }
        }
    }
    void InMemoryQueue::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        Core::sysLogger->LogInfo("cache mq", "mq thread started", "threadID", ss.str());
        Core::Message* work = nullptr;
        while (m_workQueue.Dequeue(work))
        {
            handler->Process(work);
        }
        Core::sysLogger->LogInfo("cache mq", "mq thread stopped", "threadID", ss.str());
    }

    void InMemoryQueue::Start() {
        m_threads.resize(MQ_THREADPOOL_SIZE);
        m_running.store(true, std::memory_order_relaxed);
        for (int i = 0; i < MQ_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&InMemoryQueue::ThreadFunc, this);
        }
    }


    void InMemoryQueue::Stop() {
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return;

        m_workQueue.Stop();
        for (auto& t : m_threads) {
            if (t.joinable())
                t.join();
        }
    }

    bool InMemoryQueue::EnqueueMessage(Core::Message* msg) {
        if (!m_running.load(std::memory_order_relaxed))
            return false;
        if (msg == nullptr || msg->GetLength() < sizeof(Core::MsgHeader)) {
            Core::errorLogger->LogWarn("cache mq", "invalid message");
            return false;
        }
        auto cacheMsg = messagePool->Acquire();
        if (!cacheMsg) {
            Core::errorLogger->LogError("cache mq", "message pool exhausted");
            return false;
        }
        std::memcpy(cacheMsg->GetBuffer(), msg->GetBuffer(), msg->GetLength());
        cacheMsg->SetLength(msg->GetLength());

        const Base::Priority priority = GetMessagePriority(cacheMsg);
        if (!m_running.load(std::memory_order_relaxed) || !m_workQueue.Enqueue(cacheMsg, priority)) {
            Core::errorLogger->LogWarn("cache mq", "queue rejected message", "messageType", Core::parseMsgHeader(cacheMsg->GetBuffer())->messageType, "priority", priority);
            messagePool->Return(cacheMsg);
            return false;
        }

        return true;
    }
}
