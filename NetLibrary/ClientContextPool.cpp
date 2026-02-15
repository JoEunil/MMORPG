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
            if (m_workingCnt.load() == 0 && m_flushQ.empty())
                break;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    void ClientContextPool::FlushPending() {
        while(!m_flushQ.empty())
        {
            auto t = m_flushQ.pop();
            if (t->GetWorkingCnt() != 0) {
                m_flushQ.push(t);
                break;
            }
            if (m_running) {
                m_contexts.push_back(t);
            }
        }
    }

    void ClientContextPool::Initialize()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_contexts.resize(MAX_CLIENT_CONNECTION * 3);
        for (auto& ctx : m_contexts)
            ctx = new ClientContext;
        m_running = true;
    }

    ClientContext* ClientContextPool::Acquire(uint64_t session)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_contexts.empty())
            return nullptr;
        ClientContext* ctx = m_contexts.back();
        m_contexts.pop_back();
        ctx->Clear(session);
        
        if (m_flushQ.size() > FLUSH_CONTEXTPOOL)
            FlushPending();
        return ctx;
    }

    void ClientContextPool::Return(ClientContext* context)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workingCnt.fetch_sub(1);
        context->Disconnect();
        m_flushQ.push(context);
    }
}
