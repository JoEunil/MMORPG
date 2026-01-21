#include "pch.h"
#include "ClientContextPool.h"

#include "ClientContext.h"
#include "Config.h"

namespace Net {
    void ClientContextPool::Stop() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_running = false;
            m_contexts.clear();
        }
        while (true) {
            std::unique_lock<std::mutex> lock(m_mutex);
            FlushPending();
            if (m_workingCnt.load() == 0 && m_tempQ.empty())
                break;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void ClientContextPool::FlushPending() {
        while(!m_tempQ.empty())
        {
            auto t = std::move(m_tempQ.front());
            m_tempQ.pop();
            if (t->GetWorkingCnt() != 0) {
                m_tempQ.push(std::move(t));
                break;
            }
            if (m_running) {
                m_contexts.push_back(std::move(t));
            }
        }
        if (m_contexts.size() > MAX_CONTEXTPOOL_SIZE)
            Decrease(m_contexts.size());
    }

    void ClientContextPool::Increase(uint16_t currentSize) {
        while (currentSize < TARGET_CONTEXTPOOL_SIZE)
        {
            m_contexts.emplace_back(std::make_unique<ClientContext>());
            currentSize++;
        }
    }

    void ClientContextPool::Decrease(uint16_t currentSize) {
        while (currentSize > TARGET_CONTEXTPOOL_SIZE)
        {
            m_tempQ.push(std::move(m_contexts.front()));
            m_contexts.pop_front(); 
            currentSize--;
        }
    }

    void ClientContextPool::Initialize()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i < TARGET_OVERLAPPEDPOOL_SIZE; i++)
        {
            m_contexts.emplace_back(std::make_unique<ClientContext>());
        }
        m_running = true;
    }
    std::unique_ptr<ClientContext> ClientContextPool::Acquire(SOCKET sock, uint64_t session)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_contexts.empty())
            return nullptr;
        std::unique_ptr<ClientContext> ctx = std::move(m_contexts.front());
        m_contexts.pop_front();
        ctx->Clear(sock, session);
        
        if (m_tempQ.size() > FLUSH_CONTEXTPOOL)
            FlushPending();
        
        if (m_contexts.size() < MIN_CONTEXTPOOL_SIZE)
            Increase(m_contexts.size());
        return ctx; // == std::move(ctx)
    }

    void ClientContextPool::Return(std::unique_ptr<ClientContext> context)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workingCnt.fetch_sub(1);
        context->Disconnect();
        m_tempQ.push(std::move(context));
    }
}
