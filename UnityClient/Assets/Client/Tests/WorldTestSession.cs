using System;
using ClientCore;
using ClientCore.PacketHelper;

namespace Mmorpg.Client.Tests
{
    internal static class WorldTestSession
    {
        // Synthetic metadata keeps UI lifecycle tests independent of the imported production asset.
        internal static ServerContent ItemContent()
        {
            var content = UnityEngine.ScriptableObject.CreateInstance<ServerContent>();
            content.items = new[] {
                new ServerContent.Item { itemID = 1, itemType = 1, name = "HP Potion", hpRecovery = 1500, cooldownTicks = 120 },
                new ServerContent.Item { itemID = 2, itemType = 1, name = "MP Potion", mpRecovery = 500, cooldownTicks = 120 },
                new ServerContent.Item { itemID = 3, itemType = 2, name = "Iron Sword" },
                new ServerContent.Item { itemID = 4, itemType = 2, name = "Iron Shield" },
                new ServerContent.Item { itemID = 5, itemType = 3, name = "Leather Armor" }
            };
            return content;
        }

        // Entry is ready only after the player and monster bootstrap pair is complete.
        internal static void Ready(ViewModel model, Action drain)
        {
            model.ZoneChange(0);
            model.ZoneChageReceived(1, 1, 1, 1, 0, 0, 1, 1);
            model.FullReceived(1, 1, 1, 1, true, 1, new[] { new FullStateField { zoneInternalID = 1 } });
            model.MonsterFullReceived(1, 1, 1, 1, true, 0, Array.Empty<MonsterFullField>());
            drain();
        }

        internal static void BeginAutomaticTransition(ViewModel model, Action drain, ushort targetZone = 2,
            uint internalID = 2, ulong generation = 2, ulong ownershipEpoch = 2)
        {
            model.ZoneChageReceived(1, targetZone, 2, internalID, 205, 10, generation, ownershipEpoch);
            drain();
        }

        internal static void CompleteAutomaticTransition(ViewModel model, Action drain, ushort zone = 2,
            uint internalID = 2, ulong generation = 2, ulong ownershipEpoch = 2, ulong bootstrapID = 2)
        {
            model.FullReceived(zone, generation, ownershipEpoch, bootstrapID, true, 1,
                new[] { new FullStateField { zoneInternalID = internalID, hp = 100, mp = 50, maxHp = 100, maxMp = 50, x = 205, y = 10 } });
            model.MonsterFullReceived(zone, generation, ownershipEpoch, bootstrapID, true, 0, Array.Empty<MonsterFullField>());
            drain();
        }
    }
}
