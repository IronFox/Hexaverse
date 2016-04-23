using System;
using System.Collections.Generic;
using System.Security.Cryptography;

namespace Consensus.Universe
{
	public class PlanetStructure
	{
		public readonly string Definition;
		public readonly SHA1Hash Key;
		public PlanetStructure(string def)
		{
			Definition = def;
			byte[] downCast = new byte[def.Length];
			for (int i = 0; i < def.Length; i++)
				downCast[i] = (byte)def[i];
			Key = new SHA1Hash(SHA1.Create().ComputeHash(downCast));
		}


		public static List<PlanetStructure> All = new List<PlanetStructure>();

		public static void StaticCreate()
		{

		}

		public static PlanetStructure GetRandom(Random random)
		{
			return All.Random(random);
		}
	}
}