using System.Globalization;
using UnityEngine;

namespace Mmorpg.Client
{
    // Item metadata comes from server content; icons and accents belong to presentation.
    // Unknown server IDs remain explicit; they are never assigned invented item data.
    public static class ItemPresentation
    {
        private static readonly Sprite[] itemIcons = new Sprite[5];
        private static readonly ItemDefinition[] items =
        {
            new ItemDefinition("HP", "Items/item-001-hp-potion-v1", new Color(.64f, .24f, .21f)),
            new ItemDefinition("MP", "Items/item-002-mp-potion-v1", new Color(.23f, .43f, .65f)),
            new ItemDefinition("SW", "Items/item-003-iron-sword-v1", new Color(.42f, .48f, .51f)),
            new ItemDefinition("SH", "Items/item-004-iron-shield-v1", new Color(.43f, .40f, .30f)),
            new ItemDefinition("AR", "Items/item-005-leather-armor-v1", new Color(.52f, .35f, .23f)),
        };
        private static readonly Color UnknownAccent = new Color(.40f, .43f, .39f);

        public static string Name(uint itemID, ServerContent content)
        {
            var item = content == null ? null : content.GetItem(itemID);
            return item == null ? "Item #" + itemID.ToString(CultureInfo.InvariantCulture) : item.name;
        }

        public static string Badge(uint itemID) => TryGet(itemID, out ItemDefinition item) ? item.Badge : "?";

        public static string UseDescription(uint itemID, ServerContent content)
        {
            var item = content == null ? null : content.GetItem(itemID);
            if (item == null) return "Unknown item. Use is not supported.";
            if (item.hpRecovery > 0 || item.mpRecovery > 0)
            {
                uint recovery = item.hpRecovery > 0 ? item.hpRecovery : item.mpRecovery;
                string resource = item.hpRecovery > 0 ? "HP" : "MP";
                return "Restores " + recovery.ToString(CultureInfo.InvariantCulture) + " " + resource +
                    ". Shares potion cooldown. Cannot use at full " + resource + " or while dead.";
            }
            return "Equipment cannot be used as a consumable.";
        }

        public static Sprite Icon(uint itemID)
        {
            if (!TryGet(itemID, out ItemDefinition item)) return null;
            int index = (int)itemID - 1;
            if (itemIcons[index] == null) itemIcons[index] = Resources.Load<Sprite>(item.IconPath);
            return itemIcons[index];
        }

        public static Color Accent(uint itemID) => TryGet(itemID, out ItemDefinition item) ? item.Accent : UnknownAccent;

        private static bool TryGet(uint itemID, out ItemDefinition item)
        {
            if (itemID >= 1 && itemID <= (uint)items.Length)
            {
                item = items[(int)itemID - 1];
                return true;
            }
            item = default;
            return false;
        }

        private readonly struct ItemDefinition
        {
            public readonly string Badge;
            public readonly string IconPath;
            public readonly Color Accent;

            public ItemDefinition(string badge, string iconPath, Color accent)
            {
                Badge = badge;
                IconPath = iconPath;
                Accent = accent;
            }
        }
    }
}
