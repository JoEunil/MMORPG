using System;
using ClientCore;
using ClientCore.PacketHelper;

namespace C02.Tests
{
    internal static class C08TestSession
    {
        // Existing detached tests now establish the same ACK -> own Full readiness as production.
        internal static void Ready(ViewModel model, Action drain)
        {
            model.ZoneChange(0);
            model.ZoneChageReceived(1, 1, 1, 1, 0, 0, 1);
            model.FullReceived(1, 1, 1, new[] { new FullStateField { zoneInternalID = 1 } });
            drain();
        }
    }
}
