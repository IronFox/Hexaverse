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

		internal Interface.PlanetPresence BakeFor(Faction faction)
		{
			return new Interface.PlanetPresence() { id = Planet.ID };
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

		internal Interface.SystemPresence BakeFor(Faction faction)
		{
			Interface.SystemPresence rs = new Interface.SystemPresence()
			{
				isProtectedHomeSystem = faction == SolarSystem.Owner,
				planetPresences = new Interface.PlanetPresence[planetPresences.Count]
			};
			for (int i = 0; i < planetPresences.Count; i++)
				rs.planetPresences[i] = planetPresences[i].BakeFor(faction);
			return rs;
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

		public Interface.RegionPresence Bake()
		{
			Interface.RegionPresence rs = new Interface.RegionPresence() { systemPresences = new Interface.SystemPresence[systemPresences.Count] };
			for (int i = 0; i < systemPresences.Count; i++)
				rs.systemPresences[i] = systemPresences[i].BakeFor(faction);
			return rs;
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
