using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Reflection;
using System.Reflection.Emit;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

using static ClientCore.Config;
using static ClientCore.PacketHelper.Packet;

namespace ClientCore.Network
{
    internal class Handler : IHandlerSock
    {
        private IViewModelNet _viewModel;
        internal void Inject(IViewModelNet vm)
        {
            _viewModel = vm;
        }
        public void HandleResponse(byte[] buffer)
        {
            try
            {
                var header = Packet.ExtractHeader(buffer);
                // Complete frames arrive in TCP order; each parser validates its own flags and layout.
                switch ((OP_CODE)header.opcode)
                {
                    case OP_CODE.CHAT_WHISPER:
                        HandleChatWhisper(Packet.DeserializeChatWhisper(header, buffer));
                        return;
                    case OP_CODE.CHAT_BROADCAST:
                        HandleChatBatch(Packet.DeserializeChatBatch(header, buffer));
                        return;
                    case OP_CODE.PROFILE_BATCH_RES:
                        _viewModel.ProfilesReceived(Packet.DeserializeProfiles(header, buffer));
                        return;
                    case OP_CODE.ITEM_USE_RES:
                        var use = Packet.DeserializeItemUse(header, buffer);
                        _viewModel.ItemUseReceived(use.requestID, use.ownershipEpoch, use.itemID, use.status, use.remainingQuantity, use.cooldownRemainingTicks);
                        return;
                    case OP_CODE.ITEM_REWARD:
                        var reward = Packet.DeserializeItemReward(header, buffer);
                        _viewModel.ItemRewardReceived(reward.itemID, reward.grantedQuantity, reward.remainingQuantity, reward.status);
                        return;
                    case OP_CODE.INVENTORY_RES:
                        HandleInventoryRes(Packet.DeserializeInventory(header, buffer));
                        return;
                    case OP_CODE.BAZAAR_SEARCH_RES:
                        var search = Packet.DeserializeBazaar(header, buffer);
                        _viewModel.BazaarReceived(search.requestID, search.status, search.rows);
                        return;
                    case OP_CODE.BAZAAR_BUY_RES:
                        var buy = Packet.DeserializeBazaarBuy(header, buffer);
                        _viewModel.BazaarBuyReceived(buy.requestID, buy.listingID, buy.status, buy.itemID, buy.quantity, buy.diamondSpent);
                        return;
                    case OP_CODE.BAZAAR_CHECK_OUTBOX_RES:
                        var outbox = Packet.DeserializeBazaarCheckOutbox(header, buffer);
                        _viewModel.BazaarCheckOutboxReceived(outbox.requestID, outbox.status, outbox.delivered, outbox.duplicated, outbox.blocked);
                        return;
                    case OP_CODE.BAZAAR_CANCEL_LISTING_RES:
                        var cancelled = Packet.DeserializeBazaarCancelListing(header, buffer);
                        _viewModel.BazaarCancelListingReceived(cancelled.requestID, cancelled.listingID, cancelled.status);
                        return;
                    case OP_CODE.BAZAAR_CLAIM_RES:
                        var claim = Packet.DeserializeBazaarClaim(header, buffer);
                        _viewModel.BazaarClaimReceived(claim.requestID, claim.listingID, claim.status, claim.diamondClaimed);
                        return;
                    case OP_CODE.BAZAAR_MY_LIST_RES:
                        var myList = Packet.DeserializeBazaarMyList(header, buffer);
                        _viewModel.BazaarMyListReceived(myList.requestID, myList.status, myList.rows);
                        return;
                    case OP_CODE.BAZAAR_REGISTER_RES:
                        var registered = Packet.DeserializeBazaarRegister(header, buffer);
                        _viewModel.BazaarRegisterReceived(registered.requestID, registered.status);
                        return;
                    case OP_CODE.WALLET_GOLD_RES:
                        var gold = Packet.DeserializeWalletGold(header, buffer);
                        _viewModel.WalletGoldReceived(gold.requestID, gold.status, gold.gold);
                        return;
                    case OP_CODE.WALLET_DIAMOND_RES:
                        var diamond = Packet.DeserializeWalletDiamond(header, buffer);
                        _viewModel.WalletDiamondReceived(diamond.requestID, diamond.status, diamond.diamond);
                        return;
                    case OP_CODE.ZONE_CHANGE_RESPONSE:
                        HandleZoneChangeRes(Packet.DeserializeZoneChange(header, buffer));
                        return;
                    case OP_CODE.ZONE_DELTA_UPDATE_BROADCAST:
                        HandleDeltaUpdate(Packet.DeserializeVariablePacket<DeltaUpdateField>(header, buffer));
                        return;
                    case OP_CODE.ZONE_FULL_STATE_BROADCAST:
                        HandleFullState(Packet.DeserializeFullSnapshot<FullStateField>(header, buffer));
                        return;
                    case OP_CODE.MONSTER_DELTA_UPDATE_BROADCAST:
                        HandleMonsterDelta(Packet.DeserializeVariablePacket<MonsterDeltaField>(header, buffer));
                        return;
                    case OP_CODE.MONSTER_FULL_STATE_BROADCAST:
                        HandleMonsterFull(Packet.DeserializeFullSnapshot<MonsterFullField>(header, buffer));
                        return;
                    case OP_CODE.ACTION_RESULT:
                        HandleActionResult(Packet.DeserializeVariablePacket<ActionResultField>(header, buffer));
                        return;
                    case OP_CODE.AUTH_RESPONSE:
                        HandleAuthRes(Packet.Deserialize<AuthResponseBody>(header, buffer));
                        break;
                    case OP_CODE.CHARACTER_LIST_RESPONSE:
                        HandleCharacterList(Packet.Deserialize<CharacterListResponseBody>(header, buffer));
                        break;
                    case OP_CODE.ENTER_WORLD_RESPONSE:
                        HandleEnterRes(Packet.Deserialize<EnterWorldResponseBody>(header, buffer));
                        break;
                    case OP_CODE.PING:
                        HandlePing(Packet.Deserialize<Ping>(header, buffer));
                        break;
                    default:
                        ErrorLog("Invalid opcode" + header.opcode + header.flags);
                        break;
                }
            }
            catch (Exception e)
            {
                // Malformed simulation frames now reach the same socket disconnect boundary as other responses.
                if (buffer.Length >= 7 && (buffer[6] == 7 || buffer[6] == 8 || buffer[6] == 9 || buffer[6] == 10 || buffer[6] == 12 || buffer[6] == 17 || buffer[6] == 14 || buffer[6] == 15 || buffer[6] == 21 || buffer[6] == 25 || buffer[6] == 27 || buffer[6] == 29 || buffer[6] == 31 || buffer[6] == 33 || buffer[6] == 35 || buffer[6] == 37 || buffer[6] == 41 || buffer[6] == 43 || buffer[6] == 44 || buffer[6] == 46 || buffer[6] == 47)) throw;
                ErrorLog(e.Message);
            }
        }
        private void HandleChatWhisper(Message message)
        {
            _viewModel.ChatReceived(message);
        }
        private void HandleChatBatch(Message[] messages)
        {
            foreach (Message message in messages)
            {
                _viewModel.ChatReceived(message);
            }
        }
        private void HandleAuthRes(STPacket<AuthResponseBody> packet)
        {
            _viewModel.AuthReceived(packet.body.resStatus);
        }
        private void HandleCharacterList(STPacket<CharacterListResponseBody> packet)
        {
            _viewModel.ChatacterListReceived(packet.body.resStatus, packet.body.count, packet.body.characters);
        }
        private void HandleEnterRes(STPacket<EnterWorldResponseBody> packet)
        {
            _viewModel.EnterReceived(packet.body.resStatus, packet.body.name, packet.body.attack, packet.body.level, packet.body.exp, packet.body.hp, packet.body.mp, packet.body.maxHp, packet.body.maxMp, packet.body.dir, packet.body.startX, packet.body.startY, packet.body.currentZone);
        }
        private void HandleInventoryRes(InventoryResBody body)
        {
            _viewModel.InventoryReceived(body.resStatus, body.items);
        }
        private void HandleZoneChangeRes(STPacket<ZoneChangeResponseBody> packet)
        {
            _viewModel.ZoneChageReceived(packet.body.resStatus, packet.body.zoneID, packet.body.chatID, packet.body.zoneInternalID, packet.body.startX, packet.body.startY, packet.body.zoneGeneration, packet.body.ownershipEpoch);
        }
        private void HandleDeltaUpdate((ushort count, ushort zoneID, ulong generation, DeltaUpdateField[] items) res)
        {
            _viewModel.DeltaReceived(res.zoneID, res.generation, res.count, res.items);
        }
        private void HandleFullState((ushort count, ushort zoneID, ulong generation, ulong ownershipEpoch, ulong bootstrapID, bool bootstrapEnd, FullStateField[] items) res)
        {
            _viewModel.FullReceived(res.zoneID, res.generation, res.ownershipEpoch, res.bootstrapID, res.bootstrapEnd, res.count, res.items);
        } 
        private void HandleMonsterDelta((ushort count, ushort zoneID, ulong generation, MonsterDeltaField[] items) res)
        {
            _viewModel.MonsterDeltaReceived(res.zoneID, res.generation, res.count, res.items);
        }
        private void HandleMonsterFull((ushort count, ushort zoneID, ulong generation, ulong ownershipEpoch, ulong bootstrapID, bool bootstrapEnd, MonsterFullField[] items) res)
        {
            _viewModel.MonsterFullReceived(res.zoneID, res.generation, res.ownershipEpoch, res.bootstrapID, res.bootstrapEnd, res.count, res.items);
        }
        private void HandleActionResult((ushort count, ushort zoneID, ulong generation, ActionResultField[] items) res)
        {
            _viewModel.ActionResultReceived(res.zoneID, res.generation, res.count, res.items);
        }
        public void HandlePing(STPacket<Ping> packet)
        {
            _viewModel.PingReceived(packet.body.serverTimeMs, packet.body.rtt);
        }
        private void WriteLog(string msg)
        {
            _viewModel.Log(msg);
        }

        private void ErrorLog(string msg)
        {
            _viewModel.Error(msg);
        }

    }

}
