using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Consensus.Interface;
using Consensus.Universe;

namespace Consensus.StorageItem
{
	public class PlanetPresence
	{
		public readonly OrbitingBody Planet;

		public PlanetPresence(OrbitingBody planet, Faction owner)
		{
			Planet = planet;
			planet.ActiveFactions.Add(owner);
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

		internal void Add(OrbitingBody planet, Faction owner)
		{
			foreach (var p in planetPresences)
				if (p.Planet == planet)
				{
					return;
				}
			{
				PlanetPresence p = new PlanetPresence(planet,owner);
				planetPresences.Add(p);
			}
		}
	}
	public class RegionPresence
	{
		public List<SystemPresence> systemPresences = new List<SystemPresence>();
		private Faction faction;

		public RegionPresence(Faction faction)
		{
			this.faction = faction;
		}

		internal void Add(OrbitingBody planet)
		{
			foreach (var p in systemPresences)
				if (p.SolarSystem == planet.SolarSystem)
				{
					p.Add(planet, faction);
					return;
				}
			{
				SystemPresence p = new SystemPresence(planet.SolarSystem);
				p.Add(planet, faction);
				systemPresences.Add(p);
			}
		}
	}
}
