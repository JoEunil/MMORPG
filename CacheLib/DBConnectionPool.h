#pragma once

#include <mutex>
#include <vector>
#include "DBConnection.h"
#include "Config.h"

namespace Cache {
    class DBConnectionPool {
        std::vector<DBConnection*> m_conns;
        std::mutex m_mutex;
        
        void Initialize() {
            m_conns.resize(DB_CONN_POOL);
            for (int i = 0; i < DB_CONN_POOL; i++)
            {
                m_conns[i] = new DBConnection;
                m_conns[i]->Initialize();
            }
        }
        friend class Initializer;
    public:
        DBConnection* Acquire() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_conns.empty())
                return nullptr;
            auto res = m_conns.back();
            m_conns.pop_back();
            return res;
        }
        
        void Return(DBConnection* c) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_conns.push_back(c);
        }
            
    };
    
}
