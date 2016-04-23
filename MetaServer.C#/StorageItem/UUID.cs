using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Consensus
{

	public struct UUID
	{
		public UInt64 a, b;

		public UUID(Guid guid)
		{
			byte[] ar = guid.ToByteArray();
			a = BitConverter.ToUInt64(ar, 0);
			b = BitConverter.ToUInt64(ar, 8);
		}

		Guid Guid
		{
			get
			{
				byte[] ar = new byte[16];
				BitConverter.GetBytes(a).CopyTo(ar, 0);
				BitConverter.GetBytes(b).CopyTo(ar, 8);
				return new Guid(ar);
			}
		}

		public override int GetHashCode()
		{
			return (a.GetHashCode() * 17 + b.GetHashCode());
		}

		public static bool operator ==(UUID a, UUID b)
		{
			return a.a == b.a && a.b == b.b;
		}
		public static bool operator !=(UUID a, UUID b) { return !(a == b); }

		public override bool Equals(object obj)
		{
			return obj is UUID && ((UUID)obj) == this;
		}


	}
}
