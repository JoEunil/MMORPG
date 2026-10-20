using System.Globalization;
using UnityEngine;

namespace C02
{
    // English display names for the five entries in CoreLib/ItemData.h.
    // Unknown server IDs remain explicit; they are never assigned invented item data.
    public static class C11ItemPresentation
    {
        private static readonly Sprite[] itemIcons = new Sprite[5];

        public static string Name(uint itemID) => itemID switch
        {
            1 => "HP Potion",
            2 => "MP Potion",
            3 => "Iron Sword",
            4 => "Iron Shield",
            5 => "Leather Armor",
            _ => "Item #" + itemID.ToString(CultureInfo.InvariantCulture),
        };

        public static string Badge(uint itemID) => itemID switch
        {
            1 => "HP", 2 => "MP", 3 => "SW", 4 => "SH", 5 => "AR", _ => "?",
        };

        public static Sprite Icon(uint itemID)
        {
            string path = itemID switch
            {
                1 => "Items/item-001-hp-potion-v1",
                2 => "Items/item-002-mp-potion-v1",
                3 => "Items/item-003-iron-sword-v1",
                4 => "Items/item-004-iron-shield-v1",
                5 => "Items/item-005-leather-armor-v1",
                _ => null,
            };
            if (path == null) return null;
            int index = (int)itemID - 1;
            if (itemIcons[index] == null) itemIcons[index] = Resources.Load<Sprite>(path);
            return itemIcons[index];
        }

        public static Color Accent(uint itemID) => itemID switch
        {
            1 => new Color(.64f, .24f, .21f),
            2 => new Color(.23f, .43f, .65f),
            3 => new Color(.42f, .48f, .51f),
            4 => new Color(.43f, .40f, .30f),
            5 => new Color(.52f, .35f, .23f),
            _ => new Color(.40f, .43f, .39f),
        };
    }
}
