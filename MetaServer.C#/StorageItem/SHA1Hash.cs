using System;

namespace Consensus
{
	public struct SHA1Hash
	{
		public UInt64 a, b;
		public UInt32 c;

		public SHA1Hash(byte[] byteHash)
		{
			a = BitConverter.ToUInt64(byteHash, 0);
			b = BitConverter.ToUInt64(byteHash, 8);
			c = BitConverter.ToUInt32(byteHash, 16);
		}

		public byte[] AsArray
		{
			get
			{
				byte[] rs = new byte[20];
				BitConverter.GetBytes(a).CopyTo(rs, 0);
				BitConverter.GetBytes(b).CopyTo(rs, 8);
				BitConverter.GetBytes(c).CopyTo(rs, 16);
				return rs;
			}
		}

		public override int GetHashCode()
		{
			return (a.GetHashCode()*17 + b.GetHashCode())*17 + c.GetHashCode();
		}

		public static bool operator==(SHA1Hash a, SHA1Hash b)
		{
			return a.a == b.a && a.b == b.b && a.c == b.c;
		}
		public static bool operator !=(SHA1Hash a, SHA1Hash b) { return !(a == b); }

		public override bool Equals(object obj)
		{
			return obj is SHA1Hash && ((SHA1Hash)obj) == this;
		}
	}
}