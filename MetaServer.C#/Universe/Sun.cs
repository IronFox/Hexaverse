using System;
using Math3D;

namespace Consensus.Universe
{
	public class Sun : Body
	{
		public Sun(Guid guid) : base(guid)
		{
		}

		public Vec3 Location { get; set; }

		public override Vec3 AbsoluteLocation
		{ get { return Location; } }
	}
}