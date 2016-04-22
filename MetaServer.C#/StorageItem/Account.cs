using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Xml.Linq;
using System.Collections.Concurrent;

namespace Consensus
{
    /// <summary>
    /// User account structure
    /// </summary>
    public class Account : Protocol.Account
    {
        public static ConcurrentDictionary<string,Account> Collection = new ConcurrentDictionary<string,Account>();
		public const int MaxNameLength = 32;
        
        public string CertificateHash { get; private set; }




        private static byte[] stringToByteArray(String hex)
        {
            int NumberChars = hex.Length;
            byte[] bytes = new byte[NumberChars / 2];
            for (int i = 0; i < NumberChars; i += 2)
                bytes[i / 2] = System.Convert.ToByte(hex.Substring(i, 2), 16);
            return bytes;
        }
        private static string byteArrayToString(byte[] ba)
        {
            string hex = BitConverter.ToString(ba);
            return hex.Replace("-", "");
        }

        private Account(string name, string certificateHash)
        {
			Debug.Assert(name.Length <= MaxNameLength);

            Name = name;
			CertificateHash = certificateHash;
			GUID = Guid.NewGuid();
        }
		private Account(string name, string certificateHash, Guid guid)
		{
			Debug.Assert(name.Length <= MaxNameLength);

			Name = name;
			CertificateHash = certificateHash;
			GUID = guid;
		}

        public Account(XElement xaccount)
        {
            Name = xaccount.GetString("name");
			CertificateHash = xaccount.GetString("certificateHash");
			GUID = Guid.Parse(xaccount.GetString("guid"));

			//XElement xrequests = xaccount.Element("friend_requests");
			//if (xrequests != null)
			//{
			//	foreach (XElement xrequest in xrequests.Elements())
			//		OpenFriendRequests[xrequest.GetUInt32("id")] = xrequest.Value;
			//}
			//XElement xdenies = xaccount.Element("friend_denies");
			//if (xdenies != null)
			//{
			//	foreach (XElement xdeny in xdenies.Elements())
			//		FriendRequestDenyCounter[xdeny.GetUInt32("id")] = xdeny.GetInt32("counter");
			//}
			//XElement xfriends = xaccount.Element("friends");
			//if (xfriends != null)
			//{
			//	friendIDs = new List<uint>();
			//	foreach (XElement xfriend in xfriends.Elements())
			//		friendIDs.Add(xfriend.GetUInt32("id"));
			//}

        }



        public static Account Create(string name,string certificateHash, Guid guid)
        {
			Account account = new Account(name, certificateHash, guid);
            if (Collection.TryAdd(certificateHash, account))
            {
                Continuum.Save();
                return account;
            }
            return null;
        }

  


        public static void Load(XElement data)
        {
			Account acc = new Account(data);
            Debug.Assert(Collection.TryAdd(acc.CertificateHash,acc));
        }

        public XElement ToXML()
        {
            lock (this)
            {
                XElement xaccount = new XElement("account");
                xaccount.Set("name", Name);
                xaccount.Set("certificateHash", CertificateHash);
				xaccount.Set("guid", GUID.ToString());
				
				//if (!OpenFriendRequests.IsEmpty)
				//{
				//	XElement xrequests = new XElement("friend_requests");
				//	foreach (var req in OpenFriendRequests)
				//	{
				//		XElement xrequest = new XElement("request");
				//		xrequest.Set("id", req.Key);
				//		xrequest.SetValue(req.Value);
				//		xrequests.Add(xrequest);
				//	}
				//	xaccount.Add(xrequests);
				//}
				//if (!FriendRequestDenyCounter.IsEmpty)
				//{
				//	XElement xdenies = new XElement("friend_denies");
				//	foreach (var deny in FriendRequestDenyCounter)
				//	{
				//		XElement xdeny = new XElement("deny");
				//		xdeny.Set("id", deny.Key);
				//		xdeny.Set("counter", deny.Value);
				//		xdenies.Add(xdeny);
				//	}
				//	xaccount.Add(xdenies);
				//}

				//if (Friends.Count > 0)
				//{
				//	XElement xfriends = new XElement("friends");
				//	foreach (var friend in Friends)
				//	{
				//		XElement xfriend = new XElement("friend");
				//		xfriend.Set("id", friend.ID);
				//		xfriends.Add(xfriend);
				//	}
				//	xaccount.Add(xfriends);
				//}

                return xaccount;
            }
        }

        public void PostLoadLink()
        {
			//if (friendIDs != null)
			//{
			//	foreach (UInt32 friendID in friendIDs)
			//	{
			//		Account friend = Collection.Lookup(friendID);
			//		if (friend == null)
			//			throw new Exception("Tyring to link friend avatar that does not exist: " + friendID);
			//		if (friend == this)
			//			throw new Exception("Tyring to link self as friend avatar");
			//		Friends.Add(friend);
			//	}

			//	friendIDs = null;
			//}

        }


        public override string ToString()
        {
            return ("Account('"+Name+"'/"+GUID+")");
        }

        private static char ToHex(byte b)
        {
            if (b < 10)
                return (char)('0' + b);
            return (char)('A' + (b - 10));
        }

        private static string ToHex(byte[] bytes)
        {
            char[] chars = new char[bytes.Length * 2];
            for (int i = 0; i < bytes.Length; i++)
            {
                byte b = bytes[i];
                chars[i * 2] = ToHex((byte)(b >> 4));
                chars[i * 2 + 1] = ToHex((byte)(b & 0xF));
            }
            return new string(chars);
        }

		//public byte[] HashPassword(byte[] salt)
		//{
		//	//String test = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
		//	//byte[] ascii = new byte[test.Length];
		//	//for (int i = 0; i < test.Length; i++)
		//	//    ascii[i] = (byte)test[i];
		//	//Console.Out.WriteLine(ToHex(SHA256.Create().ComputeHash(ascii)));
		//	//std::cout << bytesToHex(c.bytes,SHA256::HashLength)<<endl;

		//	//Console.Out.WriteLine("salting password " + ToHex(Password));
		//	//Console.Out.WriteLine("with salt " + ToHex(salt));

		//	byte[] salted = new byte[Password.Length + salt.Length];
		//	Array.Copy(Password, 0, salted, 0, Password.Length);
		//	Array.Copy(salt, 0, salted, Password.Length, salt.Length);
		//	//Console.Out.WriteLine("salted is " + ToHex(salted));
		//	byte[] hashed = SHA256.Create().ComputeHash(salted);
		//	//Console.Out.WriteLine("hashed is " + ToHex(hashed));

		//	return hashed;
		//}


        //private ConcurrentDictionary<uint, bool> openChannels = new ConcurrentDictionary<uint, bool>();

		//public bool IsListeningToChannel(uint channelID)
		//{
		//	return openChannels.ContainsKey(channelID);
		//}

		//public void OpenChannel(uint channelID)
		//{
		//	openChannels[channelID] = true;
		//}
		//public void CloseChannel(uint channelID)
		//{
		//	bool dummy;
		//	openChannels.TryRemove(channelID, out dummy);
		//}




		internal void ChangeName(string newNickName)
		{
			Debug.Assert(newNickName.Length <= MaxNameLength);
			this.Name = newNickName;
		}
	}
}
