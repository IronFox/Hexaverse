using System;
using Math3D;

namespace Consensus.Universe
{
	public abstract class Body
	{
		public readonly UUID ID;

		public Body(Guid guid)
		{
			ID = new UUID( guid );
		}

		public abstract Vec3 AbsoluteLocation { get; }
	}
}