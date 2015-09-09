using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Consensus
{
    namespace IO
    {
        public class CRC32
        {
            private static UInt32[] _table = null;
            private UInt32 _lastCRC = 0xFFFFFFFF;


            private static UInt32 Reflect(UInt32 reference, byte ch)
            {
                UInt32 value = 0;
                for (UInt16 i = 1; i <= (UInt16)(ch); i++)
                {
                    if ((reference & 1) != 0)
                        value |= (UInt32)1 << ((UInt16)ch - i);
                    reference >>= 1;
                }
                return value;
            }

            private static void StaticInit()
            {
                if (_table != null)
                    return;
                _table = new UInt32[0x100];
                UInt32 polynomial = 0x04C11DB7;
                for (UInt32 i = 0; i <= 0xFF; i++)
                {
                    _table[i] = Reflect(i, 8) << 24;
                    for (byte j = 0; j < 8; j++)
                        _table[i] = (_table[i] << 1) ^ ((_table[i] & ((UInt32)1 << 31)) != 0 ? polynomial : 0);
                    _table[i] = Reflect(_table[i], 32);
                }
            }

            public CRC32()
            {
                StaticInit();
            }

            private static IEnumerable<byte> EnumerateWideString(string str)
            {
                foreach (char c in str)
                {
                    UInt16 c16 = (UInt16)c;
                    yield return (byte)((c16) & 0xFF);
                    yield return (byte)((c16 >> 8) & 0xFF);
                }
            }

            public static UInt32 GetFromUnicodeString(string str)
            {
                return Get(EnumerateWideString(str));
            }

            private static IEnumerable<byte> EnumerateASCIIString(string str)
            {
                foreach (char c in str)
                {
                    UInt16 c16 = (UInt16)c;
                    yield return (byte)((c16) & 0xFF);
                }
            }

            public static UInt32 GetFromASCIIString(string str)
            {
                return Get(EnumerateASCIIString(str));
            }

            public static UInt32 Get(IEnumerable<byte> stream)
            {
                StaticInit();
                UInt32 crc = 0xFFFFFFFF;

                foreach (byte b in stream)
                {
                    crc = (crc >> 8) ^ _table[(crc & 0xFF) ^ b];
                }
                return crc ^ 0xFFFFFFFF;
            }

            public void Reset()
            {
                _lastCRC = 0xFFFFFFFF;
            }

            public CRC32 Include(IEnumerable<byte> stream)
            {
                foreach (byte b in stream)
                    _lastCRC = (_lastCRC >> 8) ^ _table[(_lastCRC & 0xFF) ^ b];
                return this;
            }

            public UInt32 GetCRC()
            {
                return _lastCRC ^ 0xFFFFFFFF;
            }

        }
    }
}
