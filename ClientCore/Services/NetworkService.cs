using ClientCore.Network;
using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using System.Data;
using System.Text;
using System.Threading.Tasks;

namespace ClientCore.Services
{
    internal class NetworkService : INetworkService
    {
        private ITCPSocket _sock;
        private ILogger _logger;
        internal NetworkService(ITCPSocket sock, ILogger logger)
        {
            _sock = sock;
            _logger = logger;
        }

        public async Task Connect(string address, int port)
        {
            await _sock.Connect(address, port);
            _sock.Send(PacketBuilder.CreateAuthPacket());
        }

        public void CharacterList()
        {
            _sock.Send(PacketBuilder.CreateCharacterListPacket());
        }

        public void Enter(ulong charID)
        {
            _sock.Send(PacketBuilder.CreateEnterWorldPacket(charID));
        }

        public void Chat(string message, byte scope, ulong targetID)
        {
            _sock.Send(PacketBuilder.CreateChatPacket(message, scope, targetID));
        }
        public void Profiles(uint[] profileIds) => _sock.Send(PacketBuilder.CreateProfileRequest(profileIds));
        public void RequestInventory() => _sock.Send(PacketBuilder.CreateInventoryRequestPacket());
        public void RequestBazaar(uint requestID, ushort itemType, ushort page) =>
            _sock.Send(Packet.SerializeBazaarRequest(requestID, itemType, page));
        public void RequestBazaarMyList(uint requestID) => _sock.Send(Packet.SerializeBazaarMyListRequest(requestID));
        public void RequestBazaarClaim(uint requestID, ulong listingID) => _sock.Send(Packet.SerializeBazaarClaimRequest(requestID, listingID));
        public void RequestBazaarBuy(uint requestID, ulong listingID) => _sock.Send(Packet.SerializeBazaarBuyRequest(requestID, listingID));
        public void RequestBazaarCheckOutbox(uint requestID) => _sock.Send(Packet.SerializeBazaarCheckOutboxRequest(requestID));
        public void RequestBazaarCancelListing(uint requestID, ulong listingID) => _sock.Send(Packet.SerializeBazaarCancelListingRequest(requestID, listingID));
        public void Action(byte dir, float x, float y, byte skillSlot, ulong ownershipEpoch)
        {
            _sock.Send(PacketBuilder.CreateActionPacket(dir, x, y, skillSlot, ownershipEpoch));
        }

        public void ZoneChange(byte op)
        {
            _sock.Send(PacketBuilder.CreateZoneChangePacket(op));
        }

        public void ZoneResync(ulong ownershipEpoch) => _sock.Send(PacketBuilder.CreateZoneResyncPacket(ownershipEpoch));
        public void RespawnToTown(ulong ownershipEpoch) => _sock.Send(PacketBuilder.CreateRespawnToTownPacket(ownershipEpoch));

        public void Pong(ulong serverTimeMs)
        {
            try
            {
                _sock.Send(PacketBuilder.CreatePongPacket(serverTimeMs));
            }
            catch (Exception ex)
            {
                Log(ex.Message);
            }
        }
        public void Log(string msg)
        {
            _logger.Log(msg);
        }
    }
}
