#pragma once

#include <memory>

#include "InMemoryQueue.h"
#include "MessagePool.h"
#include "MessageQueueHandler.h"
#include "ZoneState.h"
#include "PacketWriter.h"

#include "ILogger.h"
#include "IIOCP.h"
#include "IMessageQueue.h"
#include "StateManager.h"
#include "BroadcastThreadPool.h"
#include "ZoneHandler.h"
#include "ZoneThreadSet.h"
#include "NoneZoneHandler.h"
#include "NoneZoneThreadPool.h"
#include "PacketDispatcher.h"
#include "LobbyZone.h"
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
        NoneZoneHandler noneZoneHandler;
        NoneZoneThreadPool noneZoneThreadPool;
        PacketDispatcher packetDispatcher;
        LobbyZone lobbyZone;
        

    public:
        void Initialize() {
            msgPool.Initialize();
        }
        ~Initializer() {
            CleanUp1();
            CleanUp2();
        }
        
        void InjectDependencies1(IIOCP* iocp, ILogger* logger,IPacketPool* packetPool, IPacketPool* bigPacketPool) {
            broadcastPool.Initialize(iocp, &stateManager);
            lobbyZone.Initialize(logger, &stateManager);
            noneZoneThreadPool.Initialize(logger , &noneZoneHandler);
            zoneThreadSet.Initialize(&zoneHandler, logger);
            zoneThreadSet.Start();
            writer.Initialize(packetPool, bigPacketPool);
            mqHandler.Initialize(iocp, logger, &writer, &lobbyZone, &msgPool);
            recvMQ.Initialize(&mqHandler, &msgPool);
            recvMQ.Start();
        }
        
        void InjectDependencies2(IIOCP* iocp, ILogger* logger, ISessionAuth* session, IMessageQueue* sendMQ, IPacketPool* packetPool) {
            stateManager.Initialize(sendMQ, iocp, &msgPool, packetPool, &lobbyZone);
            stateManager.PingStart();
            packetDispatcher.Initialize(&noneZoneThreadPool, &zoneThreadSet, logger, &stateManager);
            noneZoneThreadPool.Start();
            broadcastPool.Start();
            ZoneState::Initialize(logger, &broadcastPool, &writer, &stateManager);
            zoneHandler.Initialize(logger, &stateManager);
            noneZoneHandler.Initialize(iocp, logger, session, &writer, &msgPool, sendMQ, &stateManager, &lobbyZone);
        }
        
        bool CheckReady(ILogger* logger) {
            logger->LogInfo("Core check ready start");
            if (!writer.IsReady()) {
                logger->LogError("check ready failed, writer");
                return false;
            }
            if (!msgPool.IsReady()) {
                logger->LogError("check ready failed, msgPool");
                return false;
            }
            if (!mqHandler.IsReady()) {
                logger->LogError("check ready failed, mqHandler");
                return false;
            }
            if (!recvMQ.IsReady()) {
                logger->LogError("check ready failed, recvMQ, CoreLib");
                return false;
            }
            if (!zoneHandler.IsReady()) {
                logger->LogError("check ready failed, ZoneHandler");
                return false;
            }
            if (!ZoneState::IsReady()) {
                logger->LogError("check ready failed, ZoneState");
                return false;
            }
            if (!broadcastPool.IsReady()) {
                logger->LogError("check ready failed, broadcastPool");
                return false;
            }
            if (!zoneThreadSet.IsReady()) {
                logger->LogError("check ready failed, zoneThreadSet");
                return false;
            }
            if (!lobbyZone.IsReady()) {
                logger->LogError("check ready failed, lobbyZone");
                return false;
            }
            if (!stateManager.IsReady()) {
                logger->LogError("check ready failed, stateManager");
                return false;
            }
            logger->LogWarn("Core check ready success");
        }

        IPacketDispatcher* GetPacketDispatcher() {
            return static_cast<IPacketDispatcher*>(&packetDispatcher);
        }

        IMessageQueue* GetMessageQueue() {
            return static_cast<IMessageQueue*>(&recvMQ);
        }
        
        void CleanUp1() {
            broadcastPool.Stop();
            stateManager.StopPing();
            zoneThreadSet.Stop();
        }
        void CleanUp2() {
            recvMQ.Stop();
            stateManager.CleanUp();
        }
    };
}
