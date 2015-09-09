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
    public class Account : NamedEntity
    {
        public static IDNameMap<Account> Collection = new IDNameMap<Account>();
        
        public byte[] Password { get; private set; }

        /// <summary>
        /// Thread safety:
        /// Only ever accessed by Program network events, and during World loading.
        /// Program network events are protected by a singular global lock, loading is inherently synchronized since this is done before creating any threads.
        /// If runtime access to this list is required outside Program network events, new locks must be established. Thread safety is guaranteed otherwise.
        /// </summary>
        //public List<AvatarInstance> Avatars = new List<AvatarInstance>();
        private UInt32 activeAvatarID = ID.Invalid.Value;
        private Client loggedInFrom = null;


        public List<AvatarInstance> Avatars = new List<AvatarInstance>();
        public AvatarInstance ActiveAvatar { get; private set; }
        public List<Account> Friends = new List<Account>();
        public ConcurrentDictionary<UInt32, string> OpenFriendRequests = new ConcurrentDictionary<UInt32, string>();
        public ConcurrentDictionary<UInt32, int> FriendRequestDenyCounter = new ConcurrentDictionary<UInt32, int>();
        private List<UInt32> friendIDs;

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

        public Account(string name, byte[] password)
        {
            Name = name;
            Password = password;
        }

        public Account(XElement xaccount)
        {
            Name = xaccount.GetString("name");
            Password = stringToByteArray(xaccount.GetString("password"));
            if (!xaccount.TryGetUInt32("avatar_id", out activeAvatarID))
            {
                Program.Log(null, Significance.Unusual, "Warning: Unable to restore active avatar of account " + Name);
                activeAvatarID = ID.Invalid.Value;
            }


            XElement xrequests = xaccount.Element("friend_requests");
            if (xrequests != null)
            {
                foreach (XElement xrequest in xrequests.Elements())
                    OpenFriendRequests[xrequest.GetUInt32("id")] = xrequest.Value;
            }
            XElement xdenies = xaccount.Element("friend_denies");
            if (xdenies != null)
            {
                foreach (XElement xdeny in xdenies.Elements())
                    FriendRequestDenyCounter[xdeny.GetUInt32("id")] = xdeny.GetInt32("counter");
            }
            XElement xfriends = xaccount.Element("friends");
            if (xfriends != null)
            {
                friendIDs = new List<uint>();
                foreach (XElement xfriend in xfriends.Elements())
                    friendIDs.Add(xfriend.GetUInt32("id"));
            }

        }



        /// <summary>
        /// Attempts to create a new account with the specified name and password
        /// </summary>
        /// <param name="name">Name of the account to create. Must be unique.</param>
        /// <param name="password">Password associated with the new account. Must not be null.</param>
        /// <returns>Reference to the new account or null, if an account with the specified name already exists</returns>
        public static Account Create(string name, byte[] password)
        {
            Account account = new Account(name, password);
            if (Collection.InsertNew(account))
            {
                World.Save();
                return account;
            }
            return null;
        }

        public bool IsLoggedIn { get { return loggedInFrom != null; }}
        public Client LoggedInFrom { get { return loggedInFrom; } }

        public void Activate(Client client)
        {
            loggedInFrom = client;
        }
        public void Deactivate()
        {
            lock (this)
            {
                Program.Log(loggedInFrom, Significance.Low, "Deactivated " + ToString());
                if (ActiveAvatar != null)
                {
                    ActiveAvatar.Deactivate();
                    //ActiveAvatar = null;  keep avatar 'active'
                }
                foreach (Account friend in Friends)
                {
                    lock (friend)
                    {
                        if (friend.loggedInFrom != null)
                        {
                            try
                            {
                                Interface.Out.Channel.FriendOffline.SendTo(loggedInFrom.Buffer, friend.loggedInFrom, ID.Value); //log off
                            }
                            catch { }
                        }
                    }
                }
                loggedInFrom = null;
            }
        }


        public static void Load(XElement data)
        {
            Collection.InsertExisting(data.GetUInt32("id"), new Account(data));
        }

        public XElement ToXML()
        {
            lock (this)
            {
                XElement xaccount = new XElement("account");
                xaccount.Set("id", ID);
                xaccount.Set("name", Name);
                xaccount.Set("password", byteArrayToString(Password));
                if (ActiveAvatar != null)
                    xaccount.Set("avatar_id", ActiveAvatar.ID);
                if (!OpenFriendRequests.IsEmpty)
                {
                    XElement xrequests = new XElement("friend_requests");
                    foreach (var req in OpenFriendRequests)
                    {
                        XElement xrequest = new XElement("request");
                        xrequest.Set("id", req.Key);
                        xrequest.SetValue(req.Value);
                        xrequests.Add(xrequest);
                    }
                    xaccount.Add(xrequests);
                }
                if (!FriendRequestDenyCounter.IsEmpty)
                {
                    XElement xdenies = new XElement("friend_denies");
                    foreach (var deny in FriendRequestDenyCounter)
                    {
                        XElement xdeny = new XElement("deny");
                        xdeny.Set("id", deny.Key);
                        xdeny.Set("counter", deny.Value);
                        xdenies.Add(xdeny);
                    }
                    xaccount.Add(xdenies);
                }

                if (Friends.Count > 0)
                {
                    XElement xfriends = new XElement("friends");
                    foreach (var friend in Friends)
                    {
                        XElement xfriend = new XElement("friend");
                        xfriend.Set("id", friend.ID);
                        xfriends.Add(xfriend);
                    }
                    xaccount.Add(xfriends);
                }

                return xaccount;
            }
        }

        public void PostLoadLink()
        {
            if (activeAvatarID != ID.Invalid.Value)
            {
                ActiveAvatar = AvatarInstance.Collection.Lookup(activeAvatarID);
                Debug.Assert(ActiveAvatar.Owner == this);
            }
            else
            {
                if (Avatars.Count > 0)
                {
                    ActiveAvatar = Avatars[0];
                    Debug.Assert(ActiveAvatar.Owner == this);
                }
                else
                    throw new Exception("Restored account '"+Name+"' has no active avatars.");
            }
            if (friendIDs != null)
            {
                foreach (UInt32 friendID in friendIDs)
                {
                    Account friend = Collection.Lookup(friendID);
                    if (friend == null)
                        throw new Exception("Tyring to link friend avatar that does not exist: " + friendID);
                    if (friend == this)
                        throw new Exception("Tyring to link self as friend avatar");
                    Friends.Add(friend);
                }

                friendIDs = null;
            }

        }


        public override string ToString()
        {
            return ("Account('"+Name+"'/"+ID+")");
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

        public byte[] HashPassword(byte[] salt)
        {
            //String test = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
            //byte[] ascii = new byte[test.Length];
            //for (int i = 0; i < test.Length; i++)
            //    ascii[i] = (byte)test[i];
            //Console.Out.WriteLine(ToHex(SHA256.Create().ComputeHash(ascii)));
			//std::cout << bytesToHex(c.bytes,SHA256::HashLength)<<endl;

            //Console.Out.WriteLine("salting password " + ToHex(Password));
            //Console.Out.WriteLine("with salt " + ToHex(salt));

            byte[] salted = new byte[Password.Length + salt.Length];
            Array.Copy(Password, 0, salted, 0, Password.Length);
            Array.Copy(salt, 0, salted, Password.Length, salt.Length);
            //Console.Out.WriteLine("salted is " + ToHex(salted));
            byte[] hashed = SHA256.Create().ComputeHash(salted);
            //Console.Out.WriteLine("hashed is " + ToHex(hashed));

            return hashed;
        }


        private ConcurrentDictionary<uint, bool> openChannels = new ConcurrentDictionary<uint, bool>();

        public bool IsListeningToChannel(uint channelID)
        {
            return openChannels.ContainsKey(channelID);
        }

        public void OpenChannel(uint channelID)
        {
            openChannels[channelID] = true;
        }
        public void CloseChannel(uint channelID)
        {
            bool dummy;
            openChannels.TryRemove(channelID, out dummy);
        }


        public AvatarInstance CreateAvatar(Matrix4 system, bool makeActive)
        {
            AvatarInstance avatar = new AvatarInstance(this,system);
            AvatarInstance.Collection.InsertNew(avatar);
            Structure.BVH.Lookup(system.Position, ref avatar.CurrentContexts);
            Program.Log(loggedInFrom, Significance.Low, "Set contexts of " + avatar + " to:");
            foreach (var structure in avatar.CurrentContexts)
                Program.Log(loggedInFrom, Significance.Low, "  " + structure);


            lock (Avatars)
                Avatars.Add(avatar);
            if (makeActive)
                ActiveAvatar = avatar;
            return avatar;
        }

        public void RemoveAvatar(AvatarInstance avatar)
        {
            Debug.Assert(avatar != ActiveAvatar);
            lock (Avatars)
                Avatars.Remove(avatar);
        }

        public void SwitchTo(AvatarInstance avatar)
        {
            if (ActiveAvatar != null)
                ActiveAvatar.Deactivate();
            avatar.ControlledBy = loggedInFrom;
            ActiveAvatar = avatar;
        }

    }
}
