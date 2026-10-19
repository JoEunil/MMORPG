using System.Globalization;
using UnityEngine;

namespace C02
{
    // English display names for the five entries in CoreLib/ItemData.h.
    // Unknown server IDs remain explicit; they are never assigned invented item data.
    public static class C11ItemPresentation
    {
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
