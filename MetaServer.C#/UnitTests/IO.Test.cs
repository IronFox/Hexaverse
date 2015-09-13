using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;

using Consensus.IO;

using CRC32 = Consensus.IO.CRC32;
using System.Diagnostics;
using System.Collections.Generic;


namespace UnitTests
{
    [TestClass]
    public class IOTest
    {
        [TestMethod]
        public void TestCRC32()
        {
            Dictionary<string,UInt32> asciiTests = new Dictionary<string,UInt32>
            {
                {"",0x0},
                {"a",0xE8B7BE43},
                {"abc",0x352441C2},
                {"message digest",0x20159D7F},
                {"abcdefghijklmnopqrstuvwxyz",0x4C2750BD},
                {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",0x1FC2E6D2},
                {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",0x7CA94A72},
                {"-",0x97DDB3F8},
                {"--",0x242C1465}
            };
            Dictionary<string, UInt32> unicodeTests = new Dictionary<string, UInt32>
            {
                //{"\uFFFE\u0000\u0001\u0002",0xB0E8EEE5}   //not working
            };


            foreach (var test in asciiTests)
            {
                UInt32 crc = CRC32.GetFromASCIIString(test.Key);
                Assert.AreEqual(crc, test.Value);
            }
            foreach (var test in unicodeTests)
            {
                UInt32 crc = CRC32.GetFromUnicodeString(test.Key);
                Assert.AreEqual(crc, test.Value);
            }
        }
    }
}
