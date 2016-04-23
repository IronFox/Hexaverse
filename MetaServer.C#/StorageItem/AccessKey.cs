using System;
using System.Security.Cryptography;

namespace Consensus
{
	public struct AccessKey
	{
		UInt64 a, b, c, d;

		public bool IsSet
		{
			get { return a != 0 || b != 0 || c != 0 || d != 0; }
		}

		public AccessKey(byte[] byteKey)
		{
			a = BitConverter.ToUInt64(byteKey, 0);
			b = BitConverter.ToUInt64(byteKey, 8);
			c = BitConverter.ToUInt64(byteKey, 16);
			d = BitConverter.ToUInt64(byteKey, 24);
		}

		public byte[] AsArray
		{
			get
			{
				byte[] rs = new byte[32];
				BitConverter.GetBytes(a).CopyTo(rs, 0);
				BitConverter.GetBytes(b).CopyTo(rs, 8);
				BitConverter.GetBytes(c).CopyTo(rs, 16);
				BitConverter.GetBytes(d).CopyTo(rs, 24);
				return rs;
			}
		}

		public override int GetHashCode()
		{
			return ((a.GetHashCode() * 17 + b.GetHashCode()) * 17 + c.GetHashCode()) * 17 + d.GetHashCode();
		}

		public static bool operator ==(AccessKey a, AccessKey b)
		{
			return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d;
		}
		public static bool operator !=(AccessKey a, AccessKey b) { return !(a == b); }

		public override bool Equals(object obj)
		{
			return obj is AccessKey && ((AccessKey)obj) == this;
		}
		public static AccessKey Generate()
		{
			byte[] bytes = new byte[32];
			new RNGCryptoServiceProvider().GetBytes(bytes);
			return new AccessKey(bytes);
		}
		public override string ToString()
		{
			return AsArray.ToHexString();
		}
	}
}