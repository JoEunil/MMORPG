#pragma once

#include <memory>

#include "InMemoryQueue.h"
#include "MessagePool.h"
#include "MessageQueueHandler.h"
#include "ZoneState.h"
#include "PacketWriter.h"
#include "IIOCP.h"
#include "IMessageQueue.h"
#include "StateManager.h"
#include "BroadcastThreadPool.h"
#include "ZoneHandler.h"
#include "ZoneThreadSet.h"
#include "NonZoneHandler.h"
#include "NonZoneThreadPool.h"
#include "PacketDispatcher.h"
#include "LobbyZone.h"
#include "IPingPacketWriter.h"
#include "ChatThreadPool.h"

#include "LoggerGlobal.h"

namespace Core {
    class Initializer {
        InMemoryQueue recvMQ;
        MessageQueueHandler mqHandler;
        MessagePool msgPool;
        PacketWriter writer;
        StateManager stateManager;
        BroadcastThreadPool broadcastPool;
        ZoneHandler zoneHandler;
        ZoneThreadSet zoneThreadSet;
        NonZoneHandler nonZoneHandler;
        NonZoneThreadPool nonZoneThreadPool;
        PacketDispatcher packetDispatcher;
        LobbyZone lobbyZone;
        ChatThreadPool chat;
        CorePerfCollector perfCollector;

    public:
        void Initialize() {
        }
        ~Initializer() {
            CleanUp1();
            CleanUp2();
        }
        
        void InjectDependencies1(IIOCP* iocp, IPacketPool* packetPool, IPacketPool* bigPacketPool) {
            perfCollector.Initialize();
            broadcastPool.Initialize(iocp, &stateManager, &perfCollector, &writer);
            lobbyZone.Initialize(& stateManager);
            nonZoneThreadPool.Initialize( &nonZoneHandler);
            zoneThreadSet.Initialize(&zoneHandler, &perfCollector);
            writer.Initialize(packetPool, bigPacketPool);
            mqHandler.Initialize(iocp, &writer, &lobbyZone, &msgPool);
            recvMQ.Initialize(&mqHandler, &msgPool);
            recvMQ.Start();
            chat.Initialize(iocp, &writer, &perfCollector);
        }
        
        void InjectDependencies2(IIOCP* iocp, ISessionAuth* session, IMessageQueue* sendMQ, IPacketPool* packetPool) {
            stateManager.Initialize(sendMQ, iocp, &msgPool, &lobbyZone, &chat);
            packetDispatcher.Initialize(&nonZoneThreadPool, &zoneThreadSet, &stateManager, static_cast<IPingPacketWriter*>(&writer), iocp);
            nonZoneThreadPool.Start();
            broadcastPool.Start();
            ZoneState::Initialize(&broadcastPool, &writer, &stateManager, &perfCollector);
            zoneHandler.Initialize(&stateManager);
            nonZoneHandler.Initialize(iocp, session, &writer, &msgPool, sendMQ, &stateManager, &lobbyZone, &chat);
            zoneThreadSet.Start();
            chat.Start();
            perfCollector.Start();
        }
        
        bool CheckReady() {
            if (!writer.IsReady()) {
                return false;
            }
            if (!msgPool.IsReady()) {
                return false;
            }
            if (!mqHandler.IsReady()) {
                return false;
            }
            if (!recvMQ.IsReady()) {
                return false;
            }
            if (!zoneHandler.IsReady()) {
                return false;
            }
            if (!ZoneState::IsReady()) {
                return false;
            }
            if (!broadcastPool.IsReady()) {
                return false;
            }
            if (!zoneThreadSet.IsReady()) {
                return false;
            }
            if (!lobbyZone.IsReady()) {
                return false;
            }
            if (!stateManager.IsReady()) {
                return false;
            }
            if (!chat.IsReady()) {
                return false;
            }
            if (!perfCollector.IsReady()) {
                return false;
            }
            return true;
        }

        IPacketDispatcher* GetPacketDispatcher() {
            return static_cast<IPacketDispatcher*>(&packetDispatcher);
        }

        IMessageQueue* GetMessageQueue() {
            return static_cast<IMessageQueue*>(&recvMQ);
        }

        void CleanUp1() {
            broadcastPool.Stop();
            zoneThreadSet.Stop();
            chat.Stop();
            perfCollector.Stop();
        }
        void CleanUp2() {
            recvMQ.Stop();
            stateManager.CleanUp();
        }
    };
}
