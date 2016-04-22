using System;
using Math3D;

namespace Consensus.Universe
{
	public abstract class Body
	{
		public readonly Guid GUID;

		public Body(Guid guid)
		{
			GUID = guid;
		}

		public abstract Vec3 AbsoluteLocation { get; }
	}
}