using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientCore.PacketHelper
{
    public static class EncodingHelper
    {
        internal static string DecodeUtf8(byte[] data)
        {
            int len = Array.IndexOf(data, (byte)0);
            if (len < 0) len = data.Length;
            return Encoding.UTF8.GetString(data, 0, len);
        }
        internal static string DecodeUtf8(byte[] bytes, int index, int count)
        {
            return System.Text.Encoding.UTF8.GetString(bytes, index, count);
        }

        internal static byte[] EncodeUtf8(string s)
        {
            return Encoding.UTF8.GetBytes(s);
        }
    }
}
