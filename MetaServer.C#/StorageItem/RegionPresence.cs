using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Protocol.Interface;
using Consensus.Universe;

namespace Consensus.StorageItem
{
	public class PlanetPresence
	{
		public readonly OrbitingBody Planet;
		public PlanetPresence(OrbitingBody planet)
		{
			Planet = planet;
		}

	}
	public class SystemPresence
	{
		public List<PlanetPresence> planetPresences = new List<PlanetPresence>();
		public readonly SolarSystem SolarSystem;

		public SystemPresence(SolarSystem solarSystem)
		{
			SolarSystem = solarSystem;
		}
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
