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

namespace Consensus
{




	namespace Interface
	{

		enum ChannelID
		{
			Hello,
			NewAccount,
			Authenticated,

			Count

		}


		public static class Version
		{
			public const UInt32 Combined = (UInt32)ChannelID.Count | (10 << 16);
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
				public static Protocol.OutChannel<Protocol.Interface.NewAccount> NewAccount = new Protocol.OutChannel<Protocol.Interface.NewAccount>((uint)ChannelID.NewAccount);
				public static Protocol.OutChannel<Protocol.Interface.AccountDetails> Authenticated = new Protocol.OutChannel<Protocol.Interface.AccountDetails>((uint)ChannelID.Authenticated);

				//                public static OutChannel<Struct.SystemMessage> SystemMessage = new OutChannel<Struct.SystemMessage>(ChannelID.SystemMessage);
				//public static OutChannel<Struct.PropagatedClientMessage> ClientMessage = new OutChannel<Struct.PropagatedClientMessage>(ChannelID.ClientMessage);
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
					if (!Char.IsLetterOrDigit(c))
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
				Protocol.ChannelMap.Register<UInt32>((uint)ChannelID.Hello, Event.Hello, false);
				Protocol.ChannelMap.Register<string>((uint)ChannelID.NewAccount, Event.NewAccount, false);
				//                ChannelMap.Register<Struct.FileChunkRequest>(ChannelID.FileChunkRequest, Event.FileChunkRequest);
				//              ChannelMap.Register<Struct.FriendRequest>(ChannelID.FriendRequest, Event.FriendRequest);
				//            ChannelMap.Register<UInt32>(ChannelID.FriendAccept, Event.FriendAccept);
				//            ChannelMap.Register<UInt32>(ChannelID.FriendDeny, Event.FriendDeny);
				//            ChannelMap.Register<UInt32>(ChannelID.FriendRemove, Event.FriendRemove);
			}
			public class Struct
			{
				public struct NewAccount
				{
					public string AccountName;
				}


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

				public static void NewAccount(Protocol.Client _client, string accName)
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
					Account acc2 = Account.Create(accName, cert.GetCertHashString(), guid);

					Protocol.Interface.NewAccount nacc = new Protocol.Interface.NewAccount() { details = new Protocol.Interface.AccountDetails(acc2), certificate = cert.Export(X509ContentType.Pfx) };

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

				//                private static SerialInterface contractSerializer = SerialInterface.Build(typeof(Structure.Contract.Baked));

				//private static void SendStructureToAll(Structure structure)
				//{
				//	lock (structure)
				//	{
				//		ByteBuffer contractBuffer = new ByteBuffer(1024),structureBuffer = new ByteBuffer(1024);
				//		contractSerializer.SerializePacket(Out.Channel.StructureContract.ChannelID,structure.ActiveContract.BakedVersion,contractBuffer);
				//		structureSerializer.SerializePacket(Out.Channel.StructureVisible.ChannelID,structure.Description,structureBuffer);


				//		Client.VisitAll(client =>
				//		{
				//			lock (client.HasContract)
				//			{
				//				if (!client.HasContract.ContainsKey(structure.ActiveContract.Code))
				//				{
				//					client.Transfer(contractBuffer);
				//					client.HasContract[structure.ActiveContract.Code] = true;

				//					Program.Log(client, Significance.Low, "Client does not know contract #" + structure.ActiveContract.Code + ". Sending baked version");
				//				}
				//			}
				//			client.Transfer(structureBuffer);
				//		});
				//	}
				//}


				private static string VersionToString(uint version)
				{
					return Convert.ToString(version & 0xFFFF) + ":" + Convert.ToString(version >> 16);
				}






				//public static void Login(Client client, Struct.Login login)
				//{
				//	loginGate.Pass(client);
				//	Console.Out.Write(VersionToString(login.Version));
				//	Console.Out.WriteLine("/" + login.Name);
				//	Out.Struct.LoginResponse response = new Out.Struct.LoginResponse();
				//	response.AccountID = ID.Invalid.Value;
				//	response.CurrentAvatarID = ID.Invalid.Value;
				//	response.TimeStamp = (UInt64)NetTime.Query();
				//	if (login.Version != expectedVersion)
				//	{
				//		response.Success = false;
				//		response.Message = "Incompatible protocol version detected (" + VersionToString(login.Version) + "). Expected version " + VersionToString(expectedVersion) + ". Please update your client.";
				//		Program.LogServiceDenial(client, response.Message, login.Name);
				//		Out.Channel.LoginResponse.SendTo(client, response);
				//		return;
				//	}
				//	if (client.Account != null)
				//	{
				//		response.AccountID = client.Account.ID.Value;
				//		response.CurrentAvatarID = client.Account.ActiveAvatar.ID.Value;
				//		response.Success = false;
				//		response.Message = "You are already logged in.";
				//		Program.LogServiceDenial(client, response.Message, login.Name);
				//		Out.Channel.LoginResponse.SendTo(client, response);
				//		return;
				//	}

				//	AvatarInstance incarnate = null;
				//	if (login.CreateAccount)
				//	{
				//		accountCreationGate.Pass(client);
				//		if (NameIsInvalid(login.Name, out response.Message))
				//			response.Success = false;
				//		else
				//		{
				//			Account account = Account.Create(login.Name, login.Password);
				//			if (account == null)
				//			{
				//				response.Success = false;
				//				response.Message = "The specified account name is already registered. Please check the name and try again.";
				//				Program.LogServiceDenial(client, response.Message, login.Name);
				//			}
				//			else
				//			{
				//				incarnate = account.CreateAvatar(PrimaryPlatform.Singleton.SpawnAvatarPosition(), false);
				//				Program.Log(client, Significance.Unusual, "New account " + account + " created.");
				//				Out.Channel.AvatarAppeared.Broadcast(client.Buffer, new Out.Struct.AvatarAppearance(incarnate));

				//				response.Success = true;
				//				response.Message = "Welcome to the Consensus Network.";
				//				client.Account = account;
				//				client.Account.Activate(client);
				//				response.AccountID = client.Account.ID.Value;
				//				response.CurrentAvatarID = incarnate.ID.Value;
				//				Program.Log(client, Significance.Common, "Avatar '" + login.Name + "'/" + incarnate.ID + " created at " + incarnate.System.Position + ("(spawned on primary platform)"));

				//				World.TrySave();
				//			}
				//		}
				//	}
				//	else
				//	{
				//		Account account = Account.Collection.FindByName(login.Name);
				//		if (account == null || !account.HashPassword(client.PasswordSalt).SequenceEqual(login.Password))
				//		{
				//			response.Success = false;
				//			response.Message = "Incorrect username or password.\nPlease check your credentials and try again.";
				//			Program.LogServiceDenial(client, response.Message, login.Name);
				//		}
				//		else
				//		{
				//			response.Success = true;
				//			response.Message = "";
				//			client.Account = account;
				//			client.Account.Activate(client);
				//			incarnate = account.ActiveAvatar;
				//			Debug.Assert(incarnate != null);
				//			response.AccountID = client.Account.ID.Value;
				//			response.CurrentAvatarID = incarnate.ID.Value;
				//		}
				//	}
				//	Out.Channel.LoginResponse.SendTo(client, response);

				//	if (response.Success || client.Account != null)
				//	{
				//		lock (globalChange)
				//		{
				//			Program.Log(client, Significance.Common, "Authenticated");


				//			Structure.Collection.VisitElements(structure =>
				//			{
				//				SendStructureTo(client, structure);
				//			});

				//			#if false
				//				//debug structures:
				//				PrimaryPlatform[] debugPlatforms = new PrimaryPlatform[3]{PrimaryPlatform.SpawnDebugPlatform(new Vec3(40,0,100),10),
				//													PrimaryPlatform.SpawnDebugPlatform(new Vec3(40,40,100),10),
				//													PrimaryPlatform.SpawnDebugPlatform(new Vec3(0,40,100),10)};
				//				Bridge.Node[] debugNodes = new Bridge.Node[] { new Bridge.Node(new Vec3(0, 10, 100), new Vec3(0,1,0),new Vec2(2,0)),
				//																new Bridge.Node(new Vec3(0, 30, 100), new Vec3(0,1,0),new Vec2(2,0))};
				//				Bridge.Segment[] debugSegments = new Bridge.Segment[] { new Bridge.Segment(debugNodes[0],debugNodes[1])};
				//				for (int i = 0; i < debugPlatforms.Length; i++)
				//				{
				//					var desc = debugPlatforms[i].Description;
				//					desc.ID = (uint)(1000 + i);
				//					SendStructureTo(client, debugPlatforms[i]);
				//				}
				//				for (int i = 0; i < debugNodes.Length; i++)
				//				{
				//					var desc = debugNodes[i].Description;
				//					desc.ID = (uint)(1000 + i + debugPlatforms.Length);
				//					SendStructureTo(client, debugNodes[i]);
				//				}
				//				for (int i = 0; i < debugSegments.Length; i++)
				//				{
				//					var desc = debugSegments[i].Description;
				//					debugSegments[i].OverrideNodeIndices(1000 + i + debugPlatforms.Length, 1000 + i + debugPlatforms.Length + 1);
				//					desc.ID = (uint)(1000 + i + debugPlatforms.Length + debugNodes.Length);
				//					SendStructureTo(client, debugSegments[i]);
				//				}
				//			#endif //false



				//			AvatarInstance.Collection.VisitElements(instance =>
				//				{
				//					Out.Channel.AvatarAppeared.SendTo(client, new Out.Struct.AvatarAppearance(instance));
				//					Out.Channel.AvatarConditionChange.SendTo(client, new Out.Struct.AvatarConditionChange(instance));
				//				});



				//			if (incarnate != null)
				//			{

				//				client.Account.SwitchTo(incarnate);
				//				client.ActiveAvatar = incarnate;

				//				Out.Channel.AvatarConditionChange.Broadcast(client.Buffer, new Out.Struct.AvatarConditionChange(incarnate, AvatarCondition.Active));
				//				InOut.Struct.AvatarState iresponse = new InOut.Struct.AvatarState(incarnate);
				//				Out.Channel.Incarnate.SendTo(client, iresponse);
				//			}


				//			client.StartUpdateThread();
				//		}
				//	}
				//	Debug.Assert((client.Account != null) == (client.ActiveAvatar != null));
				//	if (client.Account != null)
				//		Debug.Assert(client.Account == client.ActiveAvatar.Owner);
				//}

				//private static object avatarLock = new object();

				//public static void OpenChannel(Client client, UInt32 channelID)
				//{
				//	lock (globalChange)
				//	{
				//		if (client.Account == null)
				//		{
				//			GeneralError(client, "Unable to open channel: You are not logged in");
				//			return;
				//		}
				//		client.Account.OpenChannel(channelID);
				//		Program.Log(client, Significance.Common, "Opened channel "+channelID);
				//	}
				//}
				//public static void CloseChannel(Client client, UInt32 channelID)
				//{
				//	lock (globalChange)
				//	{
				//		if (client.Account == null)
				//			return;
				//		client.Account.CloseChannel(channelID);
				//		Program.Log(client, Significance.Common, "Closed channel " + channelID);
				//	}
				//}


				//public static void ClientMessage(Client client, Struct.ClientMessage message)
				//{
				//	messageGate.Pass(client);
				//	lock (globalChange)
				//	{
				//		if (client.Account == null)
				//		{
				//			GeneralError(client, "Unable to relay message: you are not logged in");
				//			return;
				//		}
				//		Out.Struct.PropagatedClientMessage response = new Out.Struct.PropagatedClientMessage() 
				//		{
				//			Type = message.Type,
				//			Source = new Out.Struct.AccountAvatar(client.ActiveAvatar),
				//			Message = message.Message,
				//			ChannelID = 0
				//		};

				//		switch (message.Type)
				//		{

				//			case MessageType.ChannelMessage:
				//				if (message.Target == ID.Invalid)
				//				{
				//					GeneralError(client, "Unable to relay message: 0 is not a valid channel ID");
				//				}
				//				else
				//				{
				//					response.ChannelID = message.Target;
				//					Out.Channel.ClientMessage.Broadcast(client.Buffer, response, cl => cl.IsListeningToChannel(message.Target));
				//					Program.Log(client, Significance.Common, "To channel " + message.Target + ": '" + message.Message + "'");
				//				}
				//			break;
				//			case MessageType.WorldMessage:
				//				Vec3 pos = client.ActiveAvatar.ActualPosition;

				//				Out.Channel.ClientMessage.SerialInterface.SerializePacket(Out.Channel.ClientMessage.ChannelID, response, client.Buffer);
				//				AvatarInstance.Collection.VisitElements(av =>
				//				{
				//					lock (av)
				//					{
				//						if (av.ControlledBy != null && Vec.Distance(av.ActualPosition, pos) < 50.0f)
				//							av.ControlledBy.Transfer(client.Buffer);
				//					}
				//				});
				//				Program.Log(client,Significance.Common,"To world (near "+pos+"): '"+message.Message+"'");
				//			break;
				//			case MessageType.PrivateMessage:
				//				AvatarInstance target = AvatarInstance.Collection.Lookup(message.Target);
				//				if (target == null)
				//				{
				//					GeneralError(client, "Unable to relay message: The specified target avatar does not exist.");
				//					return;
				//				}
				//				if (target.ControlledBy == null)
				//				{
				//					GeneralError(client, "Unable to relay message: "+target.Owner.Name + " is currently offline.");
				//					return;
				//				}
				//				Program.Log(client,Significance.Common,"To "+target+"("+target.Owner+"): '"+message.Message+"'");
				//				Out.Channel.ClientMessage.SendTo(client.Buffer,target.ControlledBy, response);
				//				response.ChannelID = message.Target;    //this is a confirmation
				//				Out.Channel.ClientMessage.SendTo(client, response);//echo
				//			break;
				//			default:
				//				throw new Exception("Unexpected message type encountered: " + message.Type);
				//		}

				//	}
				//}


				//public static void NameOfAccount(Client client, UInt32 id)
				//{
				//	if (client.Account == null)
				//		return;
				//	Account acc = Account.Collection.Lookup(id);
				//	if (acc != null)
				//	{
				//		Out.Struct.AccountName name = new Out.Struct.AccountName{ID = id, Name = acc.Name};
				//		Out.Channel.AccountName.SendTo(client, name);
				//	}
				//}


				//public static void FriendRemove(Client client, UInt32 accountID)
				//{
				//	if (client.ActiveAvatar == null)
				//	{
				//		GeneralError(client, "You cannot alter friendship until you have selected an avatar");
				//		return;
				//	}
				//	lock (globalChange)
				//	{
				//		Account localAccount = client.Account;
				//		Account remoteAccount = Account.Collection.Lookup(accountID);
				//		if (remoteAccount == null)
				//		{
				//			GeneralError(client, "Cannot remove friendship. The requested account (" + accountID + ") does not exist");
				//			return;
				//		}
				//		lock (localAccount)
				//		{
				//			if (!localAccount.Friends.Contains(remoteAccount))
				//			{
				//				return;
				//			}
				//			lock (remoteAccount)
				//			{
				//				localAccount.Friends.Remove(remoteAccount);
				//				remoteAccount.Friends.Remove(localAccount);
				//			}
				//		}
				//		if (remoteAccount.IsLoggedIn)
				//		{
				//			try
				//			{
				//				Out.Channel.FriendRemove.SendTo(client.Buffer, remoteAccount.LoggedInFrom, localAccount.ID.Value);
				//			}
				//			catch { }
				//		}
				//		Out.Channel.FriendRemove.SendTo(client, remoteAccount.ID.Value);
				//	}
				//	World.TrySave();
				//}

				//public static void FriendAccept(Client client, UInt32 accountID)
				//{
				//	if (client.ActiveAvatar == null)
				//	{
				//		GeneralError(client, "You cannot accept or deny friendship until you have selected an avatar");
				//		return;
				//	}
				//	Account local = client.Account;
				//	if (!local.OpenFriendRequests.ContainsKey(accountID))
				//	{
				//		GeneralError(client, "Cannot accept friendship. The requested avatar (" + accountID + ") has not requested friendship with you");
				//		return;
				//	}
				//	Account remote = Account.Collection.Lookup(accountID);
				//	if (remote == null)
				//	{
				//		GeneralError(client, "Cannot accept friendship. The requested avatar (" + accountID + ") does not exist");
				//		return;
				//	}

				//	lock (globalChange)
				//	{
				//		string dummy;
				//		lock (local)
				//			lock (remote) //global change lock should prevent deadlock here, but dual lock is required to prevent inconsistent world saving
				//			{
				//				int idummy;
				//				local.FriendRequestDenyCounter.TryRemove(accountID, out idummy);
				//				bool success = local.OpenFriendRequests.TryRemove(accountID, out dummy);
				//				Debug.Assert(success);
				//				if (!local.Friends.Contains(remote))
				//					local.Friends.Add(remote);
				//				else
				//					Debug.Fail("Remote avatar is already registered as friend");

				//				remote.FriendRequestDenyCounter.TryRemove(local.ID.Value, out idummy);
				//				remote.OpenFriendRequests.TryRemove(local.ID.Value, out dummy);
				//				if (!remote.Friends.Contains(local))
				//					remote.Friends.Add(local);
				//				else
				//					Debug.Fail("Local avatar is already registered as friend on remote avatar");

				//				Program.Log(client, Significance.Common, "Friendship accepted. Now friends with " + remote);
				//			}
				//		lock (remote)
				//		{
				//			if (remote.IsLoggedIn)
				//			{
				//				try
				//				{
				//					Out.Channel.FriendAccept.SendTo(client.Buffer, remote.LoggedInFrom, new Out.Struct.FriendAcceptNotify(client.ActiveAvatar, true));
				//				}
				//				catch { }
				//				Out.Channel.FriendOnline.SendTo(client, accountID);
				//			}
				//			else
				//				Out.Channel.FriendOffline.SendTo(client, accountID);
				//		}
				//	}
				//	World.TrySave();
				//}

				//public static void FriendDeny(Client client, UInt32 accountID)
				//{
				//	if (client.ActiveAvatar == null)
				//	{
				//		GeneralError(client, "You cannot accept or deny friendship until you have selected an avatar");
				//		return;
				//	}
				//	Account local = client.Account;
				//	if (!local.OpenFriendRequests.ContainsKey(accountID))
				//	{
				//		Program.Log(client, Significance.Unusual, "Unrequested friend ship denied to avatar " + accountID);
				//		return;
				//	}

				//	lock (globalChange)
				//	{
				//		string dummy;
				//		lock (local)
				//		{
				//			bool success = local.OpenFriendRequests.TryRemove(accountID, out dummy);
				//			Debug.Assert(success);
				//			int counter;
				//			if (!local.FriendRequestDenyCounter.TryGetValue(accountID, out counter))
				//				counter = 0;
				//			counter++;
				//			local.FriendRequestDenyCounter[accountID] = counter;
				//			Program.Log(client, Significance.Unusual, "Friendship denied. Deny counter with " + accountID + " increased to " + counter);

				//		}
				//	}
				//	World.TrySave();
				//}

				//public static void FriendRequest(Client client, In.Struct.FriendRequest request)
				//{
				//	friendRequestGate.Pass(client);
				//	if (client.ActiveAvatar == null)
				//	{
				//		GeneralError(client, "You cannot request friendship until you have selected an avatar");
				//		return;
				//	}
				//	lock (globalChange)
				//	{
				//		Account remote = Account.Collection.Lookup(request.TargetAvatar);
				//		Account local = client.Account;
				//		if (remote == null)
				//		{
				//			GeneralError(client, "Cannot request friendship. The requested avatar ("+request.TargetAvatar+") does not exist");
				//			return;
				//		}
				//		if (remote == local)
				//		{
				//			GeneralError(client, "You cannot request friendship with yourself");
				//			return;
				//		}
				//		int counter;
				//		if (remote.FriendRequestDenyCounter.TryGetValue(local.ID.Value, out counter) && counter >= 3)
				//		{
				//			//GeneralError(client, "You have been denied by "+av.ToString()+" three times. You cannot request any more often.");
				//			//silent
				//			return;
				//		}
				//		if (remote.Friends.Contains(local))
				//		{
				//			Out.Channel.FriendAccept.SendTo(client, new Out.Struct.FriendAcceptNotify(remote.ActiveAvatar, remote.IsLoggedIn));
				//			Program.Log(client, Significance.Unusual, "Double friend request received for " + remote.ToString());
				//			return;
				//		}
				//		Program.Log(client, Significance.Common, "Requesting friendship with " + remote);
				//		remote.OpenFriendRequests[local.ID.Value] = request.Message;
				//		if (remote.IsLoggedIn)
				//		{
				//			try
				//			{
				//				Out.Channel.FriendRequest.SendTo(client.Buffer, remote.LoggedInFrom, new Out.Struct.PropagatedFriendRequest(local.ActiveAvatar, request.Message));
				//				return;
				//			}
				//			catch
				//			{ }
				//		}
				//	}
				//	World.TrySave();
				//}

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
#if false
        static void LinqExample()
        {
            var numbers = new[] { 1, 5, 3, 8, 9, 2, 6, 5, 2 };

            var ascending = numbers.OrderBy(i => i);
            var descending = numbers.OrderByDescending(i => i);

            var largeNumbers = numbers.Where(i => i >= 5);
            var distinctNumbers = numbers.Distinct();

            var indexesOfLargeNumbers = numbers
                .Select((i, index) => new { Number = i, Index = index })
                .Where(i => i.Number >= 5)
                .Select(i => i.Index);

            var largeNumbers2 =
                from n in numbers
                where n >= 5
                select n;


            var sum = numbers.Sum();

            var strings = new[] {
                "Lorem",
                "Ipsum",
                "dolor",
                "sit",
                "amet",
                "consectetuer",
                "adipiscing",
                "elit",
            };
            var firstLetters = strings.Select(s => s[0]);
            var firstLetters2 = strings.FirstLetters();
            var firstLetters3 = MyExtensions.FirstLetters(strings); // Dahin übersetzt es der Compiler.

            var secondElement = strings.SecondElement();

            var distinctLetters = strings.SelectMany(s => s).Distinct();

            var distinctLetters2 = strings.SelectMany(s =>
            {
                // do something with s here
                return s;
            }).Distinct();

            var numDict = new MyNumberDictionary();
            numDict[0] = new Tuple<int, int, string>(2, 3, "Hallo");
        }

        public class MyNumberDictionary : Dictionary<int, Tuple<int, int, string>>
        {
        }
#endif

		internal static void DebugCheckGoodSystem(Matrix4 system)
		{
			//#if DEBUG           
			//    Vec3 z = Vec.Cross(system.x.xyz, system.y.xyz);
			//    Debug.Assert(z.z > 0.0f);
			//#endif
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
