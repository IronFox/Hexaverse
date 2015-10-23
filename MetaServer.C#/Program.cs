using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Xml.Linq;
using Math3D;
using System.Collections.Concurrent;
using System.IO;
using System.Security.Cryptography.X509Certificates;
using System.Security.Cryptography;

namespace Consensus
{




	namespace Interface
	{

		enum ChannelID
		{
			Hello,				//c2s : UINT32 protocol version -> [if authenticated] s2c : (Authenticated)Protocol.Interface.AccountDetails
			RequestNewAccount,	//c2s : <signal> : request new account creation -> s2c : (CreatedNewAccount)Protocol.Interface.NewAccount, s2c : (Authenticated)Protocol.Interface.AccountDetails
			CreatedNewAccount,	//s2c : Protocol.Interface.NewAccount
			Authenticated,		//s2c : Protocol.Interface.AccountDetails
			SetAccountName,		//c2s : string -> [if valid] s2c : (Authenticated)Protocol.Interface.AccountDetails, [otherwise] s2c : (BadNickname)string
			BadAccountName,		//s2c : string

			Count

		}



		public static class Version
		{
			public const UInt32 ContentVersion = 11;
			public const UInt32 Combined = (UInt32)ChannelID.Count | (ContentVersion << 16);
		}



		public static class InOut
		{

			public static TVec3<Int16> EncodeDirection(Vec3 v)
			{
				return new TVec3<Int16>((Int16)(v.x * 32767.0f), (Int16)(v.y * 32767.0f), (Int16)(v.z * 32767.0f));
			}
			public static Vec3 DecodeDirection(TVec3<Int16> v)
			{
				return new Vec3((float)v.x / 32767.0f, (float)v.y / 32767.0f, (float)v.z / 32767.0f);
			}

			public static class Struct
			{
			}
		}
		//using AccountDetails = Protocol.Interface.AccountDetails;
		public static class Out
		{

			public static class Struct
			{

				//public struct PropagatedFriendRequest
				//{
				//	public Account Source;
				//	public string Message;

				//	public PropagatedFriendRequest(Consensus.Account source, string message)
				//	{
				//		Source = new Account(source);
				//		Message = message;
				//	}
				//}

				public struct SystemMessage
				{
					//   public SystemMessageType Type;
					public string Message;
				}


			}
			public static class Channel
			{
				public static Protocol.OutChannel<Protocol.Interface.AccountDetails> Authenticated = new Protocol.OutChannel<Protocol.Interface.AccountDetails>((uint)ChannelID.Authenticated);
				public static Protocol.OutChannel<Protocol.Interface.NewAccount> NewAccount = new Protocol.OutChannel<Protocol.Interface.NewAccount>((uint)ChannelID.CreatedNewAccount);
				public static Protocol.OutChannel<string> BadAccountName = new Protocol.OutChannel<string>((uint)ChannelID.BadAccountName);

			}

		}


		public static class In
		{

			public static bool NameContainsInvalidCharacters(string name)
			{
				if (name.Length == 0)
					return true;
				if (!Char.IsLetter(name[0]))
					return true;
				foreach (char c in name)
				{
					if (!Char.IsLetterOrDigit(c) && /*!Char.IsWhiteSpace(c)*/ c != ' ' && !Char.IsPunctuation(c))
						return true;
				}
				return false;
			}

			public static bool NameIsInvalid(string name, out string errorMessage)
			{
				if (name.Length < 3)
				{
					errorMessage = "The specified name is too short. Three characters minimum.";
					return true;
				}
				if (name.Length > 32)
				{
					errorMessage = "The specified name is too long. 32 characters maximum.";
					return true;
				}
				if (NameContainsInvalidCharacters(name))
				{
					errorMessage = "The specified name contains invalid characters.";
					return true;
				}
				errorMessage = "";
				return false;
			}



			public static void Register()
			{
				Protocol.ChannelMap.Register<string>((uint)ChannelID.SetAccountName, Event.SetAccountName);
				Protocol.ChannelMap.Register<UInt32>((uint)ChannelID.Hello, Event.Hello, false);
				Protocol.ChannelMap.RegisterSignal((uint)ChannelID.RequestNewAccount, Event.RequestNewAccount, false);
				//                ChannelMap.Register<Struct.FileChunkRequest>(ChannelID.FileChunkRequest, Event.FileChunkRequest);
				//              ChannelMap.Register<Struct.FriendRequest>(ChannelID.FriendRequest, Event.FriendRequest);
				//            ChannelMap.Register<UInt32>(ChannelID.FriendAccept, Event.FriendAccept);
				//            ChannelMap.Register<UInt32>(ChannelID.FriendDeny, Event.FriendDeny);
				//            ChannelMap.Register<UInt32>(ChannelID.FriendRemove, Event.FriendRemove);
			}
			public class Struct
			{


				public struct ClientMessage
				{
					public UInt32 Target;
					public string Message;
				}
				public struct FriendRequest
				{
					public UInt32 TargetAvatar;
					public string Message;
				}
			}
			public static class Event
			{
				private static object globalChange = new object();
				private static ClientGate loginGate = new ClientGate(3),
											accountCreationGate = new ClientGate(10),
											avatarCreationGate = new ClientGate(1),
											incarnationGate = new ClientGate(0.05f),    //people can click fast
											selectAvatarGate = new ClientGate(2),
											messageGate = new ClientGate(0.25f),
											friendRequestGate = new ClientGate(1),
											structureCreationGate = new ClientGate(5),
											structureDestructionGate = new ClientGate(5),
											structureBoardGate = new ClientGate(1);

				public static object GetGlobalChangeAnchor() { return globalChange; }



				public static void Hello(Protocol.Client _client, UInt32 protocolVersion)
				{
					Client client = (Client)_client;
					loginGate.Pass(client);
					if (protocolVersion != Version.Combined)
					{
						GeneralError(client, "Version mismatch. Expected version " + VersionToString(Version.Combined) + " but got " + VersionToString(protocolVersion) + " from client");
						client.ForceDisconnect();
						return;
					}
					client.VersionMatched = true;

					if (client.Account != null)
					{
						Out.Channel.Authenticated.SendTo(client, new Protocol.Interface.AccountDetails(client.Account));
					}
				}


				private static void EncodeLength(BinaryWriter stream, int length)
				{
					if (length < 0) throw new ArgumentOutOfRangeException("length", "Length must be non-negative");
					if (length < 0x80)
					{
						// Short form
						stream.Write((byte)length);
					}
					else
					{
						// Long form
						var temp = length;
						var bytesRequired = 0;
						while (temp > 0)
						{
							temp >>= 8;
							bytesRequired++;
						}
						stream.Write((byte)(bytesRequired | 0x80));
						for (var i = bytesRequired - 1; i >= 0; i--)
						{
							stream.Write((byte)(length >> (8 * i) & 0xff));
						}
					}
				}

				private static void EncodeIntegerBigEndian(BinaryWriter stream, byte[] value, bool forceUnsigned = true)
				{
					stream.Write((byte)0x02); // INTEGER
					var prefixZeros = 0;
					for (var i = 0; i < value.Length; i++)
					{
						if (value[i] != 0) break;
						prefixZeros++;
					}
					if (value.Length - prefixZeros == 0)
					{
						EncodeLength(stream, 1);
						stream.Write((byte)0);
					}
					else
					{
						if (forceUnsigned && value[prefixZeros] > 0x7f)
						{
							// Add a prefix zero to force unsigned if the MSB is 1
							EncodeLength(stream, value.Length - prefixZeros + 1);
							stream.Write((byte)0);
						}
						else
						{
							EncodeLength(stream, value.Length - prefixZeros);
						}
						for (var i = prefixZeros; i < value.Length; i++)
						{
							stream.Write(value[i]);
						}
					}
				}

				public static void RequestNewAccount(Protocol.Client _client)
				{
					Client client = (Client)_client;
					if (!client.VersionMatched)
					{
						GeneralError(client, "Version not specified. Disconnecting client");
						client.ForceDisconnect();
						return;
					}
					accountCreationGate.Pass(client);

					//byte[] data;
					Guid guid = Guid.NewGuid();
					X509Certificate2 cert = Client.MakeCertificate("hexaverse." + guid.ToString());
					Account acc2 = Account.Create(guid.ToString(), cert.GetCertHashString(), guid);
					var rsaObj = (RSACryptoServiceProvider)cert.PrivateKey;
					byte[] key;

					var parameters = rsaObj.ExportParameters(true);
					using (var stream = new MemoryStream())
					{
						var writer = new BinaryWriter(stream);
						writer.Write((byte)0x30); // SEQUENCE
						using (var innerStream = new MemoryStream())
						{
							var innerWriter = new BinaryWriter(innerStream);
							EncodeIntegerBigEndian(innerWriter, new byte[] { 0x00 }); // Version
							EncodeIntegerBigEndian(innerWriter, parameters.Modulus);
							EncodeIntegerBigEndian(innerWriter, parameters.Exponent);
							EncodeIntegerBigEndian(innerWriter, parameters.D);
							EncodeIntegerBigEndian(innerWriter, parameters.P);
							EncodeIntegerBigEndian(innerWriter, parameters.Q);
							EncodeIntegerBigEndian(innerWriter, parameters.DP);
							EncodeIntegerBigEndian(innerWriter, parameters.DQ);
							EncodeIntegerBigEndian(innerWriter, parameters.InverseQ);
							var length = (int)innerStream.Length;
							EncodeLength(writer, length);
							writer.Write(innerStream.GetBuffer(), 0, length);
						}
						key = new byte[(int)stream.Length];
						Array.ConstrainedCopy(stream.GetBuffer(),0,key,0,key.Length);
						//key = stream.GetBuffer();
						//var base64 = Convert.ToBase64String(stream.GetBuffer(), 0, (int)stream.Length).ToCharArray();
					}


					//rsaObj.ExportParameters(true)
//					var cspBlob = rsaObj.ExportCspBlob(true);

					Protocol.Interface.NewAccount nacc = new Protocol.Interface.NewAccount() { details = new Protocol.Interface.AccountDetails(acc2), certificate = cert.Export(X509ContentType.Cert), certificateKey=key };

					Out.Channel.NewAccount.SendTo(client, nacc);
					client.Account = acc2;
					Out.Channel.Authenticated.SendTo(client, nacc.details);
					Program.Log(client, Logging.Significance.Common, "Logged in as: " + acc2.ToString());

				}

				private static void GeneralError(Client client, string error)
				{
					//                    Out.Channel.SystemMessage.SendTo(client, new Out.Struct.SystemMessage() { Type = SystemMessageType.ErrorMessage, Message = error });
					Program.LogServiceDenial(client, error);
				}


				private static string VersionToString(uint version)
				{
					return Convert.ToString(version & 0xFFFF) + ":" + Convert.ToString(version >> 16);
				}



				internal static void SetAccountName(Protocol.Client _client, string newNickName)
				{
					string error;
					if (NameIsInvalid(newNickName, out error))
					{
						Out.Channel.BadAccountName.SendTo(_client,error);
						return;
					}
					Client client = (Client)_client;
					if (client.Account == null)
					{
						throw new InvalidOperationException("Account is null");
					}
					((Account)client.Account).ChangeName(newNickName);
					Out.Channel.Authenticated.SendTo(client,new Protocol.Interface.AccountDetails(client.Account));
					Program.Log(client, Logging.Significance.Common,"Changed account name to '"+newNickName+"'");
				}
			}
		}
	}




	static class Convert
	{
		public static CultureInfo culture = new CultureInfo("en-US");

		public static float ToFloat(string str)
		{
			return System.Convert.ToSingle(str, culture);
		}
		public static uint ToUInt32(string str)
		{
			return System.Convert.ToUInt32(str, culture);
		}
		public static int ToInt32(string str)
		{
			return System.Convert.ToInt32(str, culture);
		}
		public static string ToString(float v)
		{
			return v.ToString(culture);
		}
		public static string ToString(int v)
		{
			return v.ToString(culture);
		}
		public static string ToString(uint v)
		{
			return v.ToString(culture);
		}
	}




	class Program
	{
		public const int Port = (int)Protocol.Port.Main,
						TimeServerPort = (int)Protocol.Port.Time;

		static TcpListener listener;
		static Thread listenThread;

		public static volatile bool ShuttingDown = false;


		public static void ShutDown()
		{
			ShuttingDown = true;
			if (listener != null)
				listener.Stop();
		}


		public static void ReportUnexpected(Client origin, Exception ex, Logging.Significance significance = Logging.Significance.ClientFatal)
		{
			//Debug.Fail(ex.ToString());
			Log(origin, significance, ex.ToString());
			//more later
		}
		public static void ReportCommon(Client origin, Exception ex)
		{
			//Debug.Fail(ex.ToString());
			Log(origin, Logging.Significance.Low, ex.ToString());
			//more later
		}

		public static void Log(Client origin, Logging.Significance significance, String message)
		{
			Logging.Out.Log(origin != null ? origin.ToString() : "", origin != null && origin.Account != null ? origin.Account.GUID.ToString() : "", origin != null && origin.Account != null ? origin.Account.Name : "", significance, message);

		}


		public static void ReportFatal(Client origin, Exception ex)
		{
			ReportUnexpected(origin, ex, Logging.Significance.ProgramFatal);
			ShutDown();
		}

		public static void LogServiceDenial(Client requester, String errorMessage, object additionalParameter = null)
		{
			errorMessage = "Client request denied: '" + errorMessage + "'";
			if (additionalParameter != null)
				errorMessage += " (with parameter='" + additionalParameter + "')";
			Log(requester, Logging.Significance.Unusual, errorMessage);
		}
		static void Listen()
		{
			// Is someone trying to call us? Well answer!
			Log(null, Logging.Significance.Low, "Starting listening thread on " + listener.LocalEndpoint.ToString());
			while (!ShuttingDown)
			{
				try
				{
					Client.AddNew(listener.AcceptTcpClient());
				}
				catch (InvalidOperationException ex)
				{
					if (!ShuttingDown)
						ReportFatal(null, ex);
				}
				catch (SocketException ex)
				{
					if (!ShuttingDown)
						ReportFatal(null, ex);
				}
				catch (Exception ex)
				{
					ReportUnexpected(null, ex, Logging.Significance.Unusual);
				}
			}
			Log(null, Logging.Significance.Important, "Closing down listening thread");
		}

		private static volatile bool didEnd = false;

		static void end()
		{
			listenThread.Join();
			TimeServer.End();


			lock (listenThread)
			{
				if (!didEnd)
				{
					didEnd = true;
					Debug.Assert(ShuttingDown); //this should really be implied
					ShuttingDown = true;

					Client.VisitAll(client =>
					{
						client.ShutDown();
					});

					Continuum.Save();

				}
			}
			Logging.Out.EndService();
		}

		static void onCancel(object sender, ConsoleCancelEventArgs e)
		{
			ShutDown();
			end();
		}



		static void Main(string[] args)
		{
			//string pass;
			//var cert = Client.GenSelfSignedCert("me",out pass);





			if (System.IO.Directory.Exists("./Runtime"))
				System.IO.Directory.SetCurrentDirectory("./Runtime");
			else
				System.IO.Directory.SetCurrentDirectory("../../Runtime");   //drop down from build-directory


			Console.CancelKeyPress += new ConsoleCancelEventHandler(onCancel);

			{
				XElement xconfig = XElement.Load("config.xml");

				string smode = xconfig.GetString("server_mode");
			}



			Interface.In.Register();

			//if (!World.Load())
			//  return;

			Client.InitCertificate();


			listener = new TcpListener(System.Net.IPAddress.Any, Port); //IPv4
			listener.Start();

			listenThread = new Thread(new ThreadStart(Listen));
			listenThread.Start();

			//            TimeServer.Start(TimeServerPort);
			//            ViewServer.Start(ViewServerPort);

			Logging.Out.StartService();

			System.Threading.Thread.Sleep(100);

			//World.Save();

			Log(null, Logging.Significance.Important, "Initialization phase completed. Awaiting clients.");


			end();
		}


	}
	public static class MyExtensions
	{
		public static void VisitAllElements<K, V>(this ConcurrentDictionary<K, V> dict, Action<V> op)
		{
			foreach (var entry in dict)
				op(entry.Value);

		}

		public static V GetValue<K, V>(this ConcurrentDictionary<K, V> dict, K key) where V : class
		{
			V rs;
			if (dict.TryGetValue(key, out rs))
				return rs;
			return null;
		}


		public static bool Unset<K, V>(this ConcurrentDictionary<K, V> dict, K key)
		{
			V dummy;
			return dict.TryRemove(key, out dummy);
		}

		public static IEnumerable<K> GetKeys<K, V>(this ConcurrentDictionary<K, V> dict)
		{
			foreach (var pair in dict)
				yield return pair.Key;
		}
		public static IEnumerable<V> GetValues<K, V>(this ConcurrentDictionary<K, V> dict)
		{
			foreach (var pair in dict)
				yield return pair.Value;
		}

		public static float NextFloat(this Random random)
		{
			return (float)random.NextDouble();
		}

		public static float NextIn(this Random random, Range range)
		{
			return range.Min + range.Extend * random.NextFloat();
		}


		public static void SetUInt32(this byte[] array, int address, UInt32 v)
		{
			Debug.Assert(address + 4 <= array.Length);

			array[address] = (byte)(v & 0xFF);
			array[address + 1] = (byte)((v >> 8) & 0xFF);
			array[address + 2] = (byte)((v >> 16) & 0xFF);
			array[address + 3] = (byte)((v >> 24) & 0xFF);
		}
		public static UInt32 GetUInt32(this byte[] array, int address)
		{
			Debug.Assert(address + 4 <= array.Length);

			return (UInt32)array[address]
					|
					(((UInt32)array[address + 1]) << 8)
					|
					(((UInt32)array[address + 2]) << 16)
					|
					(((UInt32)array[address + 3]) << 24);
		}
		public static void Set(this XElement element, string key, bool v)
		{
			element.SetAttributeValue(key, v ? "true" : "false");
		}
		public static void Set(this XElement element, string key, string v)
		{
			element.SetAttributeValue(key, v);
		}
		public static void Set(this XElement element, string key, float v)
		{
			element.SetAttributeValue(key, Convert.ToString(v));
		}
		public static void Set(this XElement element, string key, Vec3 v)
		{
			element.SetAttributeValue(key, Convert.ToString(v.x) + "," + Convert.ToString(v.y) + "," + Convert.ToString(v.z));
		}
		public static void Set(this XElement element, string key, Vec4 v)
		{
			element.SetAttributeValue(key, Convert.ToString(v.x) + "," + Convert.ToString(v.y) + "," + Convert.ToString(v.z) + "," + Convert.ToString(v.w));
		}

		public static void Set(this XElement element, string key, int v)
		{
			element.SetAttributeValue(key, Convert.ToString(v));
		}
		public static void Set(this XElement element, string key, uint v)
		{
			element.SetAttributeValue(key, Convert.ToString(v));
		}

		public static float GetFloat(this XElement element, string key)
		{
			return Convert.ToFloat(element.Attribute(key).Value);
		}
		public static Math3D.Vec3 GetVec3(this XElement element, string key)
		{
			string val = element.Attribute(key).Value;
			string[] elements = val.Split(new char[] { ',' });
			if (elements.Length != 3)
				throw new Exception("Expected 3 elements while parsing xml element '" + key + "' (value is '" + val + "'), but got " + elements.Length);
			return new Math3D.Vec3(Convert.ToFloat(elements[0].Trim()),
							Convert.ToFloat(elements[1].Trim()),
							Convert.ToFloat(elements[2].Trim()));
		}
		public static Math3D.Vec4 GetVec4(this XElement element, string key)
		{
			string val = element.Attribute(key).Value;
			string[] elements = val.Split(new char[] { ',' });
			if (elements.Length != 4)
				throw new Exception("Expected 4 elements while parsing xml element '" + key + "' (value is '" + val + "'), but got " + elements.Length);
			return new Math3D.Vec4(Convert.ToFloat(elements[0].Trim()),
							Convert.ToFloat(elements[1].Trim()),
							Convert.ToFloat(elements[2].Trim()),
							Convert.ToFloat(elements[3].Trim())
							);
		}

		public static Int32 GetInt32(this XElement element, string key)
		{
			return Convert.ToInt32(element.Attribute(key).Value);
		}
		public static bool GetBool(this XElement element, string key)
		{
			string value = element.Attribute(key).Value.ToLower();
			if (value == "true")
				return true;
			if (value == "false")
				return false;
			throw new Exception("Failed to decode '" + value + "' to bool");
		}
		public static UInt32 GetUInt32(this XElement element, string key)
		{
			return Convert.ToUInt32(element.Attribute(key).Value);
		}
		public static bool TryGetUInt32(this XElement element, string key, out UInt32 value)
		{
			XAttribute attrib = element.Attribute(key);
			if (attrib == null)
			{
				value = 0;
				return false;
			}
			value = Convert.ToUInt32(attrib.Value);
			return true;
		}
		public static string GetString(this XElement element, string key)
		{
			return element.Attribute(key).Value;
		}
		public static string GetOptionalString(this XElement element, string key)
		{
			var attrib = element.Attribute(key);
			if (attrib == null)
				return "";
			return attrib.Value;
		}







		public static void OutLine(this StreamWriter writer, string line)
		{
			writer.Write(line + "\n");
		}

	}
}
