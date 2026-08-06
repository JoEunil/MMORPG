using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using static ClientCore.Config;

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
        private ActionData _actionState;
        // 클라이언트가 로컬로 이동한 뒤의 최종 좌표를 기록한다.
        // 한 틱에 여러 번 호출돼도 마지막 좌표만 남으므로, 서버로는 틱당 한 번만 나간다.
        public void UpdateMove(byte dir, float x, float y)
        {
            lock (_lockMove)
            {
                _actionState.dir = dir;
                _actionState.x = x;
                _actionState.y = y;
                _actionState.dirty = true;
            }
        }
        public void UpdateSkill(byte skillSlot)
        {
            lock (_lockMove)
            {
                _actionState.waitSkillSlot = skillSlot;
                _actionState.dirty = true;
            }
        }
        // 서버가 알려준 권위 좌표로 맞춘다 (입장·존 이동).
        // 시드가 없으면 스킬만 눌렀을 때 (0,0)이 나가 영역 밖으로 거부된다.
        public void SetPosition(float x, float y)
        {
            lock (_lockMove)
            {
                _actionState.x = x;
                _actionState.y = y;
            }
        }

        public (bool, byte, float, float, byte) GetActionState()
        {
            lock (_lockMove)
            {
                var res = (_actionState.dirty, _actionState.dir, _actionState.x, _actionState.y, _actionState.waitSkillSlot);
                _actionState.waitSkillSlot = NONE_SKILL;
                _actionState.dirty = false;
                // 좌표는 이벤트가 아니라 상태이므로 초기화하지 않는다.
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
