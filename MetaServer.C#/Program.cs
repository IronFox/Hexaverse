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
using Protocol;

namespace Consensus
{


	namespace Interface
	{

		public struct AccountDetails
		{
			public string accountName;
			public UUID guid;

			public AccountDetails(Account acc)
			{
				accountName = acc.Name;
				guid = new UUID( acc.GUID );
			}
		}

		public struct PlanetPresence
		{
			public UUID id;
			//public SHA1Hash structureID;	queries elsewhere
		}
		public struct SystemPresence
		{
			public PlanetPresence[] planetPresences;
			public bool isProtectedHomeSystem;
		}
		public struct RegionPresence
		{
			public SystemPresence[] systemPresences;
		}
		public struct NewAccount
		{
			public AccountDetails details;
			public byte[] certificate;
			public byte[] certificateKey;
		}


		enum ChannelID
		{
			Hello,				//c2s : UINT32 protocol version -> [if authenticated] s2c : (Authenticated)Protocol.Interface.AccountDetails [if not authenticated] s2c : (NotAuthenticated) [if version incompatible] s2c : VersionMismatch
			VersionMismatch,	//s2c : <signal>
			RequestNewAccount,	//c2s : <signal> : request new account creation -> s2c : (CreatedNewAccount)Protocol.Interface.NewAccount, s2c : (Authenticated)Protocol.Interface.AccountDetails, s2c : (RegionPresence)Protocol.Interface.RegionPresence
			CreatedNewAccount,	//s2c : Protocol.Interface.NewAccount
			Authenticated,      //s2c : Protocol.Interface.AccountDetails, s2c : (RegionPresence)Protocol.Interface.RegionPresence
			NotAuthenticated,	//s2c : <signal>
			SetAccountName,		//c2s : string -> [if valid] s2c : (Authenticated)Protocol.Interface.AccountDetails, [otherwise] s2c : (BadNickname)string
			BadAccountName,		//s2c : string
			RegionPresence,     //s2c : Protocol.Interface.RegionPresence

			Count

		}



		public static class Version
		{
			public const UInt32 ContentVersion = 12;
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
				public static OutChannel<AccountDetails> Authenticated = new OutChannel<AccountDetails>((uint)ChannelID.Authenticated);
				public static OutChannel<NewAccount> NewAccount = new OutChannel<NewAccount>((uint)ChannelID.CreatedNewAccount);
				public static OutChannel<string> BadAccountName = new OutChannel<string>((uint)ChannelID.BadAccountName);
				public static SignalChannel VersionMismatch = new SignalChannel((uint)ChannelID.VersionMismatch);
				public static SignalChannel NotAuthenticated = new SignalChannel((uint)ChannelID.NotAuthenticated);
				public static OutChannel<RegionPresence> RegionPresence = new OutChannel<RegionPresence>((uint)ChannelID.RegionPresence);
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
				if (name.Length > Faction.MaxNameLength)
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
						Out.Channel.VersionMismatch.SendTo(client);
						Thread.Sleep(500);
						client.ForceDisconnect();
						return;
					}
					client.VersionMatched = true;
					if (client.Account != null)
					{
						Program.Log(client,Logging.Significance.Low,"hello: sending authentication info");
						Out.Channel.Authenticated.SendTo(client, new AccountDetails(client.Account));
						Out.Channel.RegionPresence.SendTo(client,client.Account.Presence.Bake());
					}
					else
					{
						Program.Log(client,Logging.Significance.Low,"hello: signalling not authenticated");
						Out.Channel.NotAuthenticated.SendTo(client);
					}
				}


				//PEM binary encoding found here:
				//http://stackoverflow.com/questions/23734792/c-sharp-export-private-public-rsa-key-from-rsacryptoserviceprovider-to-pem-strin

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
					Faction acc2 = Faction.Create(guid.ToString().Substring(0,Faction.MaxNameLength), cert.GetCertHashString(), guid);
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

					NewAccount nacc = new NewAccount() { details = new AccountDetails(acc2), certificate = cert.Export(X509ContentType.Cert), certificateKey=key };

					Out.Channel.NewAccount.SendTo(client, nacc);
					client.Account = acc2;
					acc2.Client = client;
					Out.Channel.Authenticated.SendTo(client, nacc.details);
					Program.Log(client, Logging.Significance.Common, "Logged in as: " + acc2.ToString());

					//place new account
					//let's go with a simplified single system first

					Universe.OrbitingBody leastActive = Program.RootSystem.GetLeastActivePlanet(new Random());
					client.Account.Colonize(leastActive,
						"<carrier type='CargoPlane'><unit type='Hoverformer' minerals='200' /><unit type='Hoverformer' minerals='200' /></carrier>"
						);
					Out.Channel.RegionPresence.SendTo(client, client.Account.Presence.Bake());


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
					((Faction)client.Account).ChangeName(newNickName);
					Out.Channel.Authenticated.SendTo(client,new AccountDetails(client.Account));
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

		public static Universe.SolarSystem RootSystem;

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


			Universe.PlanetStructure.StaticCreate();

			Interface.In.Register();

			//if (!World.Load())
			//  return;

			Client.StaticInitServerCertificate();


			RootSystem = new Universe.SolarSystem();
			RootSystem.Build(8, new Random());


			listener = new TcpListener(System.Net.IPAddress.Any, Port); //IPv4
			listener.Start();

			listenThread = new Thread(new ThreadStart(Listen));
			listenThread.Start();

			//            TimeServer.Start(TimeServerPort);
			//            ViewServer.Start(ViewServerPort);

			Logging.Out.StartService();

			Thread.Sleep(100);

			//World.Save();

			Log(null, Logging.Significance.Important, "Initialization phase completed. Awaiting clients.");


			end();
		}


	}
	
}
