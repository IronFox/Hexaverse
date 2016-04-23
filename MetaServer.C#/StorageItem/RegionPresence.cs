using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Protocol.Interface;

namespace Consensus.StorageItem
{
	public class PlanetPresence
	{
		public Universe.OrbitingBody planet;
	}
	public class SystemPresence
	{
		public Universe.SolarSystem system;
		public List<PlanetPresence> planetPresences = new List<PlanetPresence>();
	}
	public class RegionPresence
	{
		public List<SystemPresence> systemPresences = new List<SystemPresence>();

		internal Protocol.Interface.RegionPresence Bake()
		{
			throw new NotImplementedException();
		}
	}
}
