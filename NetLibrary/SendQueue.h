#pragma once
#include <mutex>

#include "STOverlappedEx.h"
#include "OverlappedExPool.h"
#include "Config.h"

#include <BaseLib/RingQueue.h>


namespace Net
{
    enum class EnqueueSendResult
    {
        Ready,      // 큐가 비어있어 즉시 전송 가능
        Queued,     // pending으로 큐에 적재됨 
        QueueFull,   // 큐 가득참 
        Failed     // 실패
    };
    class SendQueue {
        Base::RingQueue<STOverlappedEx*, SEND_QUEUE_SIZE> m_sendQueue;
        std::mutex m_mutex;
        bool m_sendPending = false;

    public:
        EnqueueSendResult Enqueue(STOverlappedEx* work) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_sendPending == false) {

                m_sendPending = true;
                return EnqueueSendResult::Ready;
            }
            if (!m_sendQueue.push(work))
                return EnqueueSendResult::QueueFull;
            return EnqueueSendResult::Queued;
        }

        STOverlappedEx* Dequeue() {
            std::lock_guard<std::mutex> lock(m_mutex);
            STOverlappedEx* res = nullptr;
            if (!m_sendQueue.pop(res)) {
                m_sendPending = false;
                return nullptr;
            }
            return res;
        }
        void Clear(OverlappedExPool* overlappedExPool) {
            std::lock_guard<std::mutex> lock(m_mutex);
            STOverlappedEx* pending = nullptr;
            while (m_sendQueue.pop(pending))
                overlappedExPool->Return(pending);
            m_sendPending = false;
        }
    };
}