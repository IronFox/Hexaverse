using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Math3D;

namespace Consensus.Universe
{
	public class OrbitingBody : Body
	{
		public readonly Matrix3 RotationSystem = Matrix3.Identity;  //these may become mutable at some point
		public readonly float	OrbitalRadius;
		public readonly Body	CentralBody;
		public readonly PlanetStructure InitialStructure;
		public readonly SolarSystem SolarSystem;
		public float Angle { get; private set; }

		public List<Faction> ActiveFactions { get; private set; } = new List<Faction>();

		public Vec3 RelativeLocation
		{
			get
			{
				return RotationSystem * new Vec3((float)Math.Sin(Angle) * OrbitalRadius, (float)Math.Cos(Angle) * OrbitalRadius, 0f);
			}
		}

		public override Vec3 AbsoluteLocation
		{
			get
			{
				return CentralBody.AbsoluteLocation + RelativeLocation;
			}
		}


		public OrbitingBody(SolarSystem system, PlanetStructure initialStructure, Body centralBody, float orbitalRadius, Guid guid, float initialAngle) : base(guid)
		{
			OrbitalRadius = orbitalRadius;
			CentralBody = centralBody;
			Angle = initialAngle;
			InitialStructure = initialStructure;
			SolarSystem = system;
		}
		}
	}
}
