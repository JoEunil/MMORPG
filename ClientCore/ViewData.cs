using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace ClientCore
{
    internal class ViewData: IViewDataModel, IViewDataUI
    {
        private readonly object _lockServerTime = new object();
        private long _serverTimeMs;
        public long GetServerTimeMs()
        {
            lock (_lockServerTime)
            {
                return _serverTimeMs;
            }
        }
        public void UpdateServerTime(long serverTime)
        {
            lock (_lockServerTime)
            {
                _serverTimeMs = serverTime;
            }
        }

        private readonly object _lockMove = new object();
        private MoveData _moveState;
        public void UpdateMove(byte dir, float speed)
        {
            lock (_lockMove)
            {
                _moveState.dir = dir;
                _moveState.speed = speed;
                _moveState.dirty = true;
            }
        }
        public (bool, byte, float) GetMoveState()
        {
            lock (_lockMove)
            {
                var res = (_moveState.dirty, _moveState.dir, _moveState.speed);
                _moveState.dirty = false;
                return res;
            }
        }

        private readonly object _lockCharList = new object();
        private List<CharacterInfoView> _charList = new List<CharacterInfoView>();
        private ushort _charListCnt = 0;
        public void SetCharList(ushort count, CharacterInfo[] chars)
        {
            lock (_lockCharList)
            {
                _charListCnt = count;
                _charList.Clear();
                foreach (var c in chars)
                {
                    string name = PacketHelper.EncodingHelper.DecodeUtf8(c.name);
                    _charList.Add(new CharacterInfoView
                    {
                        CharacterID = c.characterID,
                        Name = name,
                        Level = c.level,
                    });
                }
            }
        }

        public (ushort, List<CharacterInfoView>) GetCharList()
        {
            lock (_lockCharList)
            {
                return (_charListCnt, _charList);
            }
        }

        private readonly object _lockInventory = new object();
        private List<InventoryItemView> _inventory = new List<InventoryItemView>();

        public void SetInventory(InventoryItem[] items)
        {
            lock (_lockInventory)
            {
                _inventory.Clear();
                foreach (var item in items)
                {
                    _inventory.Add(new InventoryItemView
                    {
                        ItemID = item.itemID,
                        Quantity = item.quantity,
                        Slot = item.slot,
                    });
                }
            }
        }
        public List<InventoryItemView> GetInventory()
        {
            lock (_lockInventory)
            {
                return _inventory;
            }
        }
    }
}
