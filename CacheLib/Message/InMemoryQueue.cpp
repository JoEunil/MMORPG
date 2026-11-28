#include "pch.h"
#include "InMemoryQueue.h"
#include "MessageDispatcher.h"

namespace Cache {
    namespace {
        Base::Priority GetMessagePriority(const Core::IMessage* message) {
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
            case Core::MSG_DIAMOND_DEPOSIT:
            case Core::MSG_BAZAAR_REGISTER:
            case Core::MSG_BAZAAR_CANCEL:
            case Core::MSG_BAZAAR_BUY:
            case Core::MSG_BAZAAR_CLAIM:
            case Core::MSG_BAZAAR_CHECK_OUTBOX:
            case Core::MSG_BAZAAR_CANCEL_LISTING:
            case Core::MSG_PROFILE_RENAME:
            case Core::MSG_INVENTORY_UPDATE:
                return Base::Priority::Droppable;
            case Core::MSG_CURRENCY_DEPOSIT:
            case Core::MSG_CHARACTER_STATE_UPDATE:
                return Base::Priority::Important;
            default:
                Core::errorLogger->LogWarn("cache mq", "unknown message priority", {{"messageType", Core::parseMsgHeader(message->GetBuffer())->messageType}});
                return Base::Priority::Important;
            }
        }
    }

    bool InMemoryQueue::WriteDurable(const Core::IMessage* message) {
        if (message == nullptr || message->GetLength() < sizeof(Core::MsgHeader)) {
            Core::errorLogger->LogError("cache mq", "durable write failed - invalid message");
            return false;
        }

        const auto header = Core::parseMsgHeader(message->GetBuffer());
        if (durableLogger != nullptr && durableLogger->Write(Core::DurableLogType::CacheMessage, message->GetBuffer(), message->GetLength())) return true;

        if (header->messageType == Core::MSG_CHARACTER_STATE_UPDATE) {
            std::vector<std::byte> binary(message->GetLength());
            std::memcpy(binary.data(), message->GetBuffer(), message->GetLength());
            Core::errorLogger->LogError("cache mq", "disconnect durable write failed", {{"sessionID", header->sessionID}, {"character state", binary}});
        } else {
            Core::errorLogger->LogError("cache mq", "durable write failed", {{"sessionID", header->sessionID}, {"messageType", header->messageType}});
        }
        return false;
    }

    void InMemoryQueue::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        Core::sysLogger->LogInfo("cache mq", "mq thread started", {{"threadID", ss.str()}});
        Core::ScopedMessage work;
        while (m_workQueue.Dequeue(work))
        {
            messageDispatcher->Process(std::move(work));
        }
        Core::sysLogger->LogInfo("cache mq", "mq thread stopped", {{"threadID", ss.str()}});
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

    Base::EnqueueResult InMemoryQueue::EnqueueMessage(const Core::IMessage* msg) {
        if (!m_running.load(std::memory_order_relaxed))
            return Base::EnqueueResult::Rejected;
        if (msg == nullptr || msg->GetLength() < sizeof(Core::MsgHeader) || msg->GetLength() > msg->GetCapacity()) {
            Core::errorLogger->LogWarn("cache mq", "invalid message");
            return Base::EnqueueResult::Rejected;
        }
        const Base::Priority priority = GetMessagePriority(msg);
        auto cacheMsg = messagePool->AcquireScoped();
        if (!cacheMsg) {
            if (priority == Base::Priority::Important && WriteDurable(msg))
                return Base::EnqueueResult::RecordedForRecovery;
            Core::errorLogger->LogError("cache mq", "message pool exhausted");
            return Base::EnqueueResult::Rejected;
        }
        if (cacheMsg->GetCapacity() < msg->GetLength()) {
            if (priority == Base::Priority::Important && WriteDurable(msg))
                return Base::EnqueueResult::RecordedForRecovery;
            Core::errorLogger->LogError("cache mq", "message exceeds receiver capacity");
            return Base::EnqueueResult::Rejected;
        }
        std::memcpy(cacheMsg->GetBuffer(), msg->GetBuffer(), msg->GetLength());
        cacheMsg->SetLength(msg->GetLength());

        const auto result = m_running.load(std::memory_order_relaxed)
            ? m_workQueue.Enqueue(cacheMsg, priority) : Base::EnqueueResult::Rejected;
        if (result == Base::EnqueueResult::Rejected) {
            const auto messageType = Core::parseMsgHeader(cacheMsg->GetBuffer())->messageType;
            if (priority == Base::Priority::Important)
                Core::errorLogger->LogError("cache mq", "queue rejected important message", {{"messageType", messageType}});
            else
                Core::errorLogger->LogWarn("cache mq", "queue rejected message", {{"messageType", messageType}});
            return Base::EnqueueResult::Rejected;
        }

        return result;
    }
}
