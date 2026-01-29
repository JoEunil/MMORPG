#include "pch.h"
#include "ClientContextPool.h"

#include "ClientContext.h"
#include "Config.h"

namespace Net {
    void ClientContextPool::Stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
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
            auto t = m_tempQ.front();
            m_tempQ.pop();
            if (t->GetWorkingCnt() != 0) {
                m_tempQ.push(t);
                break;
            }
            if (m_running) {
                m_contexts.push_back(t);
            }
        }
        Decrease(m_contexts.size());
    }

    void ClientContextPool::Increase(uint16_t currentSize) {
        while (currentSize < TARGET_CONTEXTPOOL_SIZE)
        {
            m_contexts.push_back(new ClientContext);
            currentSize++;
        }
    }

    void ClientContextPool::Decrease(uint16_t currentSize) {
        while (currentSize > TARGET_CONTEXTPOOL_SIZE)
        {
            auto t = m_contexts.front();
            delete t;
            m_contexts.pop_front(); 
            currentSize--;
        }
    }

    void ClientContextPool::Initialize()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_contexts.resize(TARGET_OVERLAPPEDPOOL_SIZE);
        for (auto& ctx : m_contexts)
            ctx = new ClientContext;
        m_running = true;
    }
    ClientContext* ClientContextPool::Acquire(uint64_t session)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_contexts.empty())
            return nullptr;
        ClientContext* ctx = m_contexts.front();
        m_contexts.pop_front();
        ctx->Clear(session);
        
        if (m_tempQ.size() > FLUSH_CONTEXTPOOL)
            FlushPending();
        Increase(m_contexts.size());
        return ctx;
    }

    void ClientContextPool::Return(ClientContext* context)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workingCnt.fetch_sub(1);
        context->Disconnect();
        m_tempQ.push(context);
    }
}
