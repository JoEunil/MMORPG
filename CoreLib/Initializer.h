#pragma once

#include <memory>

#include "Cache/CacheCommandSender.h"
#include "Response/ServerResponseWorker.h"
#include "Cache/InMemoryQueue.h"
#include "Cache/MessagePool.h"
#include "Cache/CacheResponseHandler.h"
#include "Cache/MessageWriter.h"
#include "World/ZoneState.h"
#include "Packet/PacketWriter.h"
#include "Interface/INetworkSender.h"
#include "Interface/IMessageQueue.h"
#include "World/SessionManager.h"
#include "World/BroadcastThreadPool.h"
#include "World/ZoneEventRouter.h"
#include "World/ZoneHandler.h"
#include "World/ZoneEventHandler.h"
#include "NonSimulation/NonSimulationDispatcher.h"
#include "NonSimulation/SessionLifecycleHandler.h"
#include "NonSimulation/WorldEntryHandler.h"
#include "NonSimulation/BazaarRequestHandler.h"
#include "NonSimulation/ZoneRequestHandler.h"
#include "NonSimulation/PlayerDataRequestHandler.h"
#include "NonSimulation/ChatRequestHandler.h"
#include "NonSimulation/NonSimulationThreadPool.h"
#include "Packet/PacketDispatcher.h"
#include "World/LobbyZone.h"
#include "Interface/IPingPacketWriter.h"
#include "NonSimulation/ChatThreadPool.h"
#include "Interface/IProfileCache.h"
#include "Interface/IInventoryOperations.h"
#include "Cache/DurableLogger.h"

#include "LoggerGlobal.h"

namespace Core {
    class Initializer {
        CoreMessagePool msgPool;
        InMemoryQueue recvMQ;
        CacheResponseHandler cacheResponseHandler;
        PacketWriter writer;
        MessageWriter messageWriter;
        CacheCommandSender cacheCommandSender;
        ZoneEventHandler zoneEventHandler;
        ZoneHandler zoneHandler;
        ZoneEventRouter zoneEventRouter;
        ChatThreadPool chat;
        SessionManager sessionManager;
        BroadcastThreadPool broadcastPool;
        ServerResponseWorker responses;
        SessionLifecycleHandler sessionLifecycleHandler;
        WorldEntryHandler worldEntryHandler;
        BazaarRequestHandler bazaarHandler;
        ZoneRequestHandler zoneRequestHandler;
        PlayerDataRequestHandler playerDataHandler;
        ChatRequestHandler chatRequestsHandler;
        NonSimulationDispatcher nonSimulationDispatcher;
        NonSimulationThreadPool nonSimulationThreadPool;
        PacketDispatcher packetDispatcher;
        LobbyZone lobbyZone;
        CorePerfCollector perfCollector;

    public:
        void Initialize() {
        }
        ~Initializer() {
            StopProducers();
            FinishShutdown();
        }
        
        void InjectDependencies1(INetworkSender* networkSender, IPacketPoolManager* packetPoolManager) {
            perfCollector.Initialize();
            broadcastPool.Initialize(networkSender, zoneEventRouter, &perfCollector, &writer);
            lobbyZone.Initialize(& sessionManager);
            nonSimulationThreadPool.Initialize(&nonSimulationDispatcher);
            writer.Initialize(packetPoolManager);
            responses.Initialize(*networkSender, zoneEventRouter);
            responses.Start();
            cacheResponseHandler.Initialize(responses, writer, &lobbyZone, &sessionManager, zoneEventRouter);
            recvMQ.Initialize(&cacheResponseHandler, &msgPool);
            recvMQ.Start();
            chat.Initialize(networkSender, &writer, &perfCollector);
        }
        
        void InjectDependencies2(INetworkSender* networkSender, ISessionAuth* session, IMessageQueue* sendMQ, IProfileCache* profileCache, IInventoryOperations* inventory = nullptr) {
            cacheCommandSender.Initialize(sendMQ, &messageWriter, &msgPool);
            zoneEventRouter.Initialize(zoneHandler, perfCollector, nonSimulationThreadPool);
            sessionManager.Initialize(chat);
            packetDispatcher.Initialize(nonSimulationThreadPool, zoneEventRouter, sessionManager);
            nonSimulationThreadPool.Start();
            broadcastPool.Start();
            ZoneState::Initialize(&broadcastPool, &writer, &sessionManager, &perfCollector, cacheCommandSender, responses, inventory);
            zoneEventHandler.Initialize(sessionManager, lobbyZone, zoneEventRouter, zoneEventRouter, cacheCommandSender, responses, writer);
            zoneHandler.Initialize(&sessionManager, responses, writer, zoneEventRouter, zoneEventRouter, zoneEventHandler);
            sessionLifecycleHandler.Initialize(sessionManager, lobbyZone, zoneEventRouter, cacheCommandSender, responses, writer);
            worldEntryHandler.Initialize(&responses, &writer, session, &cacheCommandSender, &sessionManager);
            bazaarHandler.Initialize(&responses, &writer, &cacheCommandSender, &sessionManager);
            zoneRequestHandler.Initialize(&responses, &writer, &sessionManager, &lobbyZone, &zoneEventRouter, &zoneEventRouter);
            playerDataHandler.Initialize(&responses, &writer, &cacheCommandSender, &sessionManager, profileCache);
            chatRequestsHandler.Initialize(&sessionManager, &chat);
            nonSimulationDispatcher.Initialize(worldEntryHandler, bazaarHandler, zoneRequestHandler, playerDataHandler, chatRequestsHandler, sessionLifecycleHandler);
            zoneEventRouter.Start();
            chat.Start();
            perfCollector.Start();
        }
        
        bool CheckReady() {
            if (!writer.IsReady()
                or !msgPool.IsReady()
                or !responses.IsReady()
                or !cacheResponseHandler.IsReady()
                or !recvMQ.IsReady()
                or !zoneEventHandler.IsReady()
                or !zoneHandler.IsReady()
                or !sessionLifecycleHandler.IsReady()
                or !worldEntryHandler.IsReady()
                or !bazaarHandler.IsReady()
                or !zoneRequestHandler.IsReady()
                or !playerDataHandler.IsReady()
                or !chatRequestsHandler.IsReady()
                or !nonSimulationDispatcher.IsReady()
                or !ZoneState::IsReady()
                or !broadcastPool.IsReady()
                or !zoneEventRouter.IsReady()
                or !packetDispatcher.IsReady()
                or !lobbyZone.IsReady()
                or !cacheCommandSender.IsReady()
                or !sessionManager.IsReady()
                or !chat.IsReady()
                or !perfCollector.IsReady()) {
                return false;
            }
            return true;
        }

        IPacketDispatcher* GetPacketDispatcher() {
            return static_cast<IPacketDispatcher*>(&packetDispatcher);
        }

        IPingPacketWriter* GetPingPacketWriter() {
            return &writer;
        }

        IMessageQueue* GetMessageQueue() {
            return static_cast<IMessageQueue*>(&recvMQ);
        }

        void StopProducers() {
            nonSimulationThreadPool.Stop();
            zoneEventRouter.Stop();
            broadcastPool.Stop();
            chat.Stop();
            perfCollector.Stop();
        }
        void FinishShutdown() {
            recvMQ.Stop();
            responses.Stop();
            CharacterState character;
            while (lobbyZone.TakeCharacter(character))
                cacheCommandSender.EnqueueCharacterStateUpdate(character.sessionID, character);
            for (const auto& transfer : sessionManager.Shutdown())
                cacheCommandSender.EnqueueCharacterStateUpdate(transfer->character.sessionID, transfer->character);
        }
    };
}
