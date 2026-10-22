using System;
using ClientCore;
using ClientCore.PacketHelper;

namespace C02.Tests
{
    internal static class C08TestSession
    {
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
