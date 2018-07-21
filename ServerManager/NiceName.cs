using System;
using System.IO;

namespace ServerManager
{
	public static class NiceName
	{
		private static string[] nouns;
		public static void Load()
		{
			nouns = File.ReadAllLines(Path.Combine("client", "resources", "nice", "nouns.txt"));
		}



		public static string Get(ulong code)
		{
			string result = nouns[((UInt32)code) % nouns.Length];
			code >>= 32;
			byte ch = (byte)((code & 0xff) % 26);
			result += ' ';
			result += (char)('A' + ch);
			result += ((code >> 8) % 100).ToString();
			return result;
		}
	}
}