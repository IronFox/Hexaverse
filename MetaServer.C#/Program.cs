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
using Consensus;
using System.Collections.Concurrent;

namespace Consensus
{
    public enum ProgramMode
    {
        Research,
        Game
    }




    namespace Interface
    {


        public enum ChannelID
        {
            //these must be first, always!
            //server -> client
            PasswordSalt,        //(BYTE[]: key to use as salt for password hashing)
            //client -> server
            Login,				//(Network::Login) response: Login (LoginResponse), SystemMessage (String: message/error)


            //common channels, may be re-ordered. Update content version if you do

            //client -> server
            CreateAvatar,		//(AvatarState) response: CreateAvatar(Network::AvatarAppear) response: to all: AvatarAppeared(Network::AvatarAppear)
            DestroyAvatar,	    //(UINT32: id) response: to all: AvatarVanished(UINT32: id)
            Incarnate,			//(UINT32: avatar id) response: Incarnate (AvatarState) to all: 2x AvatarConditionChange (AvatarConditionChange)
            ResetMyAvatar,	    //() response: Incarnate (AvatarState)
            UpdateMyAvatar,	    //(Network::DynamicAvatarState)	response: UpdateMyAvatar()
            NameOfAccount,		//(UINT32: id) response: NameOfAccount (Network::AccountName: name)
            AccountOfAvatar,	//(UINT32: id) response: AccountAvatar (Network::AccountAvatar: name)
            ClientMessage,		//(Network::ClientMessage) response: to some: ClientMessage (Network::PropagatedClientMessage: message)
            OpenChannel,		//(UINT32: id) no response
            CloseChannel,		//(UINT32: id) no response
            FriendRequest,		//(Network::FriendRequest) response: to target avatar FriendRequest (Network::PropagatedFriendRequest)
            FriendAccept,		//(UINT32: avatar_id) response: to target avatar FriendAccept (Network::FriendAcceptNotify)
            FriendDeny,         //(UINT32: avatar_id)
            FriendRemove,		//(UINT32: avatar_id) response: FriendRemove(UINT32: avatar_id)
            FileInfoRequest,    //(UINT32: file_id) response: FileInfoRequest(FileInfo)
            FileChunkRequest,   //(FileChunkRequest) response: FileChunk(FileChunk)
            
            CreateStructure,	//(CreateStructure)	response: to all: StructureVisible(Structure), StructuresUpdate(StructureFrame), possibly AvatarConditionChange(AvatarConditionChange)
            EnterStructure,		//(UINT32: structure_id) response: to all: AvatarConditionChange(AvatarConditionChange)
            ExitStructure,		//() response: ExitStructure(), to all: AvatarConditionChange(AvatarConditionChange)
            DestroyStructure,	//(UINT32: id)
            UpdateStructure,	//(StructureState)	response: UpdateStructure(UINT32)




            //server -> client
            SystemMessage,		//(Network::SystemMessage: message)
            /*Login*/            //(LoginResponse)
            /*CreateAvatar*/        //(Network::AvatarAppear)
            /*Incarnate*/           //(AvatarState)
            /*UpdateMyAvatar*/      //()
            /*NameOfAccount*/       //(Network::AccountName: name)
            /*AccountOfAvatar*/     //(Network::AccountAvatar)
            /*ClientMessage*/       //(Network::PropagatedClientMessage: message
            /*FriendRequest*/       //(Network::PropagatedFriendRequest) response: possibly FriendAccept(UINT32: avatar id)
            /*FriendAccept*/        //(Network::FriendAcceptNotify)
            /*FriendRemove*/        //(UINT32: avatar_id)
            /*StructureContract*/   //(Network::StructureContract) no response

            FriendOnline,		//(UINT32: friend avatar that has come online)
            FriendOffline,		//(UINT32: friend avatar that has gone offline)
            AvatarAppeared,	    //(Network::Avatar: avatar that has been been created, or all if newly logged in)
            AvatarsUpdated,	    //(AvatarFrame: avatars that have been updated)
            AvatarVanished,	        //(UINT32: avatar id)
            AvatarConditionChange,//(AvatarConditionChange)
            StructureContract,  //(Network::StructureContract)
            StructureVisible,		//(Network::Structure: structure that was newly created, updated, or if user is newly logged in)
            StructureVanished,		//(UINT32: structure that has been removed during the frame, or become invisible due to other reasons)
            StructuresUpdated,	//(StructureFrame)
            FileChunk,          //(FileChunk)
            
            OpenView,           //(ViewDescriptor)
            CloseView,          //(UINT32)
            UpdateViewLocation, //(ViewLocation)
            ChangeView,         //(UINT32)
            ActivateView,       //(UINT32)
            DeactivateView,     //(UINT32)

            Count		//must remain last
	    };

        public enum Content
        {
            Version = 10	//must be incremented whenever the type of data being transfered along any of the above channels changes.
        }

        public enum Port
        {
            Main = 12874,
            Logger = 12879,
            Time = 12880,
            View = 12881
        }


        public enum MessageType
        {
            WorldMessage,
            ChannelMessage,
            PrivateMessage
        }

        public enum SystemMessageType
        {
            ErrorMessage,
            GeneralMessage
        }

        public enum AvatarCondition
        {
            Idle,
            Active
        };



        public static class InOut
        {
            private static UInt32 IDValueOf(Entity e)
            {
                if (e == null)
                    return ID.Invalid.Value;
                return e.ID.Value;
            }

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
                public class AvatarState
                {
                    const UInt32 FlagMask = 0x00FFFFFF;
                    const UInt32 DistanceMask = 0xFF000000;

                    public UInt32       ID;
                    public UInt32       FlagsAndCameraDistance;
                    public TVec3<Int16> OrientationX,
                                        OrientationY;
                    public Vec3         Position;

                    public AvatarState()
                    { }

                    public AvatarState(AvatarInstance av)
                    {
                        ID = av.ID.Value;
                        FlagsAndCameraDistance = (av.GetFlags() & FlagMask) | (((UInt32)av.ThirdPersonDistance) << 24);
                        OrientationX = EncodeDirection(av.System.x.xyz);
                        OrientationY = EncodeDirection(av.System.y.xyz);
                        Program.DebugCheckGoodSystem(av.System);
                        Position = av.System.Position;
                    }

                    public AvatarState(Structure.RealtimeControl strct)
                    {
                        ID = strct.ID.Value;
                        FlagsAndCameraDistance = strct.GetFlags();
                        OrientationX = EncodeDirection(strct.System.x.xyz);
                        OrientationY = EncodeDirection(strct.System.y.xyz);
                        Program.DebugCheckGoodSystem(strct.System);
                        Position = strct.System.Position;
                    }

                    public Matrix4 ToSystem()
                    {
                        Matrix4 result = new Matrix4(1);
                        result.x.xyz = DecodeDirection(OrientationX).Normalize();
                        result.y.xyz = DecodeDirection(OrientationY).Normalize();
                        result.z.xyz = Vec.Cross(result.x.xyz, result.y.xyz).Normalize();
                        result.Position = Position;
                        Program.DebugCheckGoodSystem(result);
                        return result;
                    }
                }

                public class DynamicAvatarState : AvatarState
                {
                    public Vec3     LinearVelocity;
                    public UInt64   ServerTime;

                    public DynamicAvatarState()
                    { }
                    public DynamicAvatarState(AvatarInstance av) : base(av)
                    {
                        ServerTime = (ulong)av.LastCaptured;
                        LinearVelocity = av.LinearVelocity;
                    }
                    public DynamicAvatarState(Structure.RealtimeControl strct)
                        : base(strct)
                    {
                        ServerTime = (ulong)strct.Captured;
                        LinearVelocity = strct.LinearVelocity;
                    }
                }

                public class StaticAvatarState : AvatarState
                {
                    public UInt64 LastActive;
                    public UInt32 AccountID;

                    public StaticAvatarState()
                    { }
                    public StaticAvatarState(AvatarInstance av)
                        : base(av)
                    {
                        LastActive = LocalTime.UnixTime();
                        AccountID = av.Owner.ID.Value;
                    }
                }

                public class StructureState : DynamicAvatarState
                {
                    public Vec3 Momentum;

                    public StructureState()
                    { }
                    public StructureState(Structure.RealtimeControl strct)
                        : base(strct)
                    {
                        Momentum = strct.Momentum;
                    }
                }

            }
        }
        public static class Out
        {
            private static UInt32 IDValueOf(Entity e)
            {
                if (e == null)
                    return ID.Invalid.Value;
                return e.ID.Value;
            }

            public static class Struct
            {
                public struct LoginResponse
                {
                    public bool Success;
                    public UInt32   AccountID,
                                    CurrentAvatarID;
                    public string Message;
                    public UInt64 TimeStamp;
                }

                public struct AccountName
                {
                    public UInt32 ID;
                    public string Name;
                }

                public struct AccountAvatar
                {
                    public UInt32   AvatarID,
                                    AccountID;
                    public string   Name;

                    public AccountAvatar(AvatarInstance avatar)
                    {
                        AccountID = avatar.Owner.ID.Value;
                        AvatarID = avatar.ID.Value;
                        Name = avatar.Owner.Name;
                    }
                }
                
                public struct PropagatedClientMessage
                {
                    public MessageType Type;
                    public AccountAvatar Source;
                    public UInt32 ChannelID;    //targeted channel ID, or (special usage) targeted avatar if private message. If zero, this is an authentic message, if not zero, then this is an echo
                    public string Message;
                }

                public struct PropagatedFriendRequest
                {
                    public AccountAvatar Source;
                    public string Message;

                    public PropagatedFriendRequest(AvatarInstance source, string message)
                    {
                        Source = new AccountAvatar(source);
                        Message = message;
                    }
                }

                public struct FriendAcceptNotify
                {
                    public AccountAvatar Source;
                    public bool CurrentlyOnline;

                    public FriendAcceptNotify(AvatarInstance newFriend, bool isOnline)
                    {
                        Source = new AccountAvatar(newFriend);
                        CurrentlyOnline = isOnline;
                    }
                }

                public struct InstanceFrame
                {
                    public byte Serial;
                    public InOut.Struct.DynamicAvatarState[] InstanceList;
                }

                public struct StructureFrame
                {
                    public byte Serial;
                    public InOut.Struct.StructureState[] StructureList;
                }

                public struct SystemMessage
                {
                    public SystemMessageType Type;
                    public string Message;
                }

                public struct AvatarAppearance
                {
                    public InOut.Struct.StaticAvatarState State;
                    public UInt32 AvatarID;
                    public UInt32[] Contexts;

                    public AvatarAppearance(AvatarInstance avatar)
                    {
                        AvatarID = avatar.ID.Value;
                        State = new InOut.Struct.StaticAvatarState(avatar);
                        Contexts = avatar.BakeCurrentContextIDs();
                    }
                }

                public struct ConsensusFileInfo
                {
                    public string MIMEType;
                    public UInt32 Size;
                    public UInt32 CRC32;
                    public UInt32 NumChunks;

                    public ConsensusFileInfo(FileObject obj)
                    {
                        MIMEType = obj.MIMEType;
                        Size = obj.FileSize;
                        CRC32 = obj.CRC32;
                        NumChunks = obj.NumChunks;
                    }
                };

                public struct FileInfo
                {
                    public UInt32 FileID;
                    public string URL;
                    public ConsensusFileInfo ConsensusFileInfo;
                };

                public struct FileChunk
                {
                    public In.Struct.FileChunkRequest Request;
                    public byte[] Data;

                    public FileChunk(In.Struct.FileChunkRequest request, byte[] data)
                    {
                        Request = request;
                        Data = data;
                    }
                };



                public struct AvatarConditionChange
                {
                    public AvatarCondition NewCondition;
                    public UInt32[] CurrentContexts;
                    public InOut.Struct.AvatarState State;
                    public UInt32 CurrentStructureID;

                    public AvatarConditionChange(AvatarInstance av, AvatarCondition condition)
                    {
                        NewCondition = condition;
                        State = new InOut.Struct.AvatarState(av);
                        CurrentContexts = av.BakeCurrentContextIDs();
                        CurrentStructureID = av.BoardedStructure != null ? av.BoardedStructure.ID.Value : ID.Invalid.Value;
                        //Console.Out.WriteLine("encoded platform id as "+CurrentPlatform);
                    }
                    public AvatarConditionChange(AvatarInstance av)
                        : this(av, av.ControlledBy != null ? AvatarCondition.Active: AvatarCondition.Idle)
                    { }
                }


                public struct ViewDescriptor
                {
                    public UInt32   AccountID;
                    public UInt32   ViewID,
                                    StructureID;

                    public ViewDescriptor(Account account, uint viewID, uint structID)
                    {
                        AccountID = account.ID.Value;
                        ViewID = viewID;
                        StructureID = structID;
                    }
                }

                public struct ViewLocation
                {
                    public UInt32 ViewID;
                    public float Relative;

                    public ViewLocation(UInt32 viewID, float relative)
                    {
                        ViewID = viewID;
                        Relative = relative;
                    }
                }
            }
            public static class Channel
            {
                public static OutChannel<byte[]> PasswordSalt = new OutChannel<byte[]>(ChannelID.PasswordSalt);
                public static OutChannel<Struct.SystemMessage> SystemMessage = new OutChannel<Struct.SystemMessage>(ChannelID.SystemMessage);
                public static OutChannel<Struct.LoginResponse> LoginResponse = new OutChannel<Struct.LoginResponse>(ChannelID.Login);
                public static OutChannel<Structure.Descriptor> StructureVisible = new OutChannel<Structure.Descriptor>(ChannelID.StructureVisible);
                public static OutChannel<Struct.StructureFrame> StructuresUpdated = new OutChannel<Struct.StructureFrame>(ChannelID.StructuresUpdated);
                public static OutChannel<UInt32> StructureVanished = new OutChannel<UInt32>(ChannelID.StructureVanished);
                public static SignalChannel ConfirmUpdateInstance = new SignalChannel(ChannelID.UpdateMyAvatar);
                public static SignalChannel ConfirmStructureUpdateInstance = new SignalChannel(ChannelID.UpdateStructure);
                public static OutChannel<InOut.Struct.AvatarState> Incarnate = new OutChannel<InOut.Struct.AvatarState>(ChannelID.Incarnate);
                public static OutChannel<Struct.AvatarAppearance> AvatarAppeared = new OutChannel<Struct.AvatarAppearance>(ChannelID.AvatarAppeared);
                public static OutChannel<Struct.InstanceFrame> AvatarsUpdated = new OutChannel<Struct.InstanceFrame>(ChannelID.AvatarsUpdated);
                public static OutChannel<UInt32> AvatarVanished = new OutChannel<UInt32>(ChannelID.AvatarVanished);
                public static OutChannel<Struct.AvatarConditionChange> AvatarConditionChange = new OutChannel<Struct.AvatarConditionChange>(ChannelID.AvatarConditionChange);
                public static OutChannel<Struct.AccountName> AccountName = new OutChannel<Struct.AccountName>(ChannelID.NameOfAccount);
                public static OutChannel<Struct.AccountAvatar> AccountOfAvatar = new OutChannel<Struct.AccountAvatar>(ChannelID.AccountOfAvatar);
                public static OutChannel<Struct.PropagatedClientMessage> ClientMessage = new OutChannel<Struct.PropagatedClientMessage>(ChannelID.ClientMessage);
                public static OutChannel<Struct.FileInfo> FileInfo = new OutChannel<Struct.FileInfo>(ChannelID.FileInfoRequest);
                public static OutChannel<Struct.FileChunk> FileChunk = new OutChannel<Struct.FileChunk>(ChannelID.FileChunk);
                public static OutChannel<Struct.PropagatedFriendRequest> FriendRequest = new OutChannel<Struct.PropagatedFriendRequest>(ChannelID.FriendRequest);
                public static OutChannel<Struct.FriendAcceptNotify> FriendAccept = new OutChannel<Struct.FriendAcceptNotify>(ChannelID.FriendAccept);
                public static OutChannel<UInt32> FriendRemove = new OutChannel<UInt32>(ChannelID.FriendRemove);
                public static OutChannel<UInt32> FriendOnline = new OutChannel<UInt32>(ChannelID.FriendOnline);
                public static OutChannel<UInt32> FriendOffline = new OutChannel<UInt32>(ChannelID.FriendOffline);
                public static OutChannel<Structure.Contract.Baked> StructureContract = new OutChannel<Structure.Contract.Baked>(ChannelID.StructureContract);
                public static OutChannel<Struct.ViewDescriptor> OpenView = new OutChannel<Struct.ViewDescriptor>(ChannelID.OpenView);
                public static OutChannel<Struct.ViewLocation> UpdateViewLocation = new OutChannel<Struct.ViewLocation>(ChannelID.UpdateViewLocation);
                public static OutChannel<UInt32> CloseView = new OutChannel<uint>(ChannelID.CloseView);
                public static OutChannel<UInt32> ChangeView = new OutChannel<uint>(ChannelID.ChangeView);
                public static OutChannel<UInt32> ActivateView = new OutChannel<uint>(ChannelID.ActivateView);
                public static OutChannel<UInt32> DeactivateView = new OutChannel<uint>(ChannelID.DeactivateView);
            }

        }


        public static class In
        {
            private static UInt32 IDValueOf(Entity e)
            {
                if (e == null)
                    return ID.Invalid.Value;
                return e.ID.Value;
            }

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
                ChannelMap.Register<Struct.Login>(ChannelID.Login, Event.Login,false);
                ChannelMap.Register<InOut.Struct.AvatarState>(ChannelID.CreateAvatar, Event.CreateAvatarInstance);
                ChannelMap.Register<UInt32>(ChannelID.DestroyAvatar, Event.DestroyAvatarInstance);
                ChannelMap.Register<UInt32>(ChannelID.Incarnate, Event.Incarnate);
                ChannelMap.RegisterSignal(ChannelID.ResetMyAvatar, Event.ResetMyInstance);
                ChannelMap.Register<InOut.Struct.DynamicAvatarState>(ChannelID.UpdateMyAvatar, Event.UpdateMyInstance);
                ChannelMap.Register<UInt32>(ChannelID.NameOfAccount, Event.NameOfAccount);
                ChannelMap.Register<byte>(ChannelID.AvatarsUpdated, Event.InstancesUpdated);
                ChannelMap.Register<byte>(ChannelID.StructuresUpdated, Event.StructuresUpdated);
                ChannelMap.Register<Struct.ClientMessage>(ChannelID.ClientMessage, Event.ClientMessage);
                ChannelMap.Register<UInt32>(ChannelID.OpenChannel, Event.OpenChannel);
                ChannelMap.Register<UInt32>(ChannelID.CloseChannel, Event.CloseChannel);
                ChannelMap.Register<UInt32>(ChannelID.FileInfoRequest, Event.FileInfoRequest);
                ChannelMap.Register<Struct.FileChunkRequest>(ChannelID.FileChunkRequest, Event.FileChunkRequest);
                ChannelMap.Register<Struct.FriendRequest>(ChannelID.FriendRequest, Event.FriendRequest);
                ChannelMap.Register<UInt32>(ChannelID.FriendAccept, Event.FriendAccept);
                ChannelMap.Register<UInt32>(ChannelID.FriendDeny, Event.FriendDeny);
                ChannelMap.Register<UInt32>(ChannelID.FriendRemove, Event.FriendRemove);
                ChannelMap.Register<Struct.CreateStructure>(ChannelID.CreateStructure, Event.CreateStructure);
                ChannelMap.Register<UInt32>(ChannelID.EnterStructure, Event.EnterStructure);
                ChannelMap.RegisterSignal(ChannelID.ExitStructure, Event.ExitStructure);
                ChannelMap.Register<UInt32>(ChannelID.DestroyStructure, Event.DestroyStructure);
            }
            public class Struct
            {
                public struct Login
                {
                    public UInt32 Version;
                    public string Name;
                    public byte[] Password;
                    public bool CreateAccount;
                }
                public struct ClientMessage
                {
                    public MessageType     Type;
                    public UInt32   Target;
                    public string   Message;
                }
                public struct FileChunkRequest
                {
                    public UInt32   FileID;
                    public UInt32   ChunkIndex;
                }
                public struct FriendRequest
                {
                    public UInt32 TargetAvatar;
                    public string Message;
                }

                public struct CreateStructure
                {
                    public Structure.Contract.Baked Contract;
                    public Structure.Descriptor Structure;
                    public InOut.Struct.StructureState State;
                    public bool Board;
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
                private const UInt16 contentVersion = (UInt16)Content.Version;
                private const UInt32 expectedVersion = (UInt32)ChannelID.Count << 16 | contentVersion;

                public static object GetGlobalChangeAnchor() { return globalChange; }

                private static void GeneralError(Client client, string error)
                {
                    Out.Channel.SystemMessage.SendTo(client, new Out.Struct.SystemMessage() { Type = SystemMessageType.ErrorMessage, Message = error });
                    Program.LogServiceDenial(client, error);
                }

                private static void SendStructureTo(Client client, Structure structure)
                {
                    lock (structure)
                    {
                        lock (client.HasContract)
                        {
                            if (!client.HasContract.ContainsKey(structure.ActiveContract.Code))
                            {
                                Out.Channel.StructureContract.SendTo(client, structure.ActiveContract.BakedVersion);
                                client.HasContract[structure.ActiveContract.Code] = true;

                                Program.Log(client, Significance.Low, "Client does not know contract #" + structure.ActiveContract.Code + ". Sending baked version");
                            }
                        }
                        Out.Channel.StructureVisible.SendTo(client, structure.Description);
                    }
                }

                private static SerialInterface contractSerializer = SerialInterface.Build(typeof(Structure.Contract.Baked));
                private static SerialInterface structureSerializer = SerialInterface.Build(typeof(Structure.Descriptor));

                private static void SendStructureToAll(Structure structure)
                {
                    lock (structure)
                    {
                        ByteBuffer contractBuffer = new ByteBuffer(1024),structureBuffer = new ByteBuffer(1024);
                        contractSerializer.SerializePacket(Out.Channel.StructureContract.ChannelID,structure.ActiveContract.BakedVersion,contractBuffer);
                        structureSerializer.SerializePacket(Out.Channel.StructureVisible.ChannelID,structure.Description,structureBuffer);


                        Client.VisitAll(client =>
                        {
                            lock (client.HasContract)
                            {
                                if (!client.HasContract.ContainsKey(structure.ActiveContract.Code))
                                {
                                    client.Transfer(contractBuffer);
                                    client.HasContract[structure.ActiveContract.Code] = true;

                                    Program.Log(client, Significance.Low, "Client does not know contract #" + structure.ActiveContract.Code + ". Sending baked version");
                                }
                            }
                            client.Transfer(structureBuffer);
                        });
                    }
                }


                private static string VersionToString(uint version)
                {
                    return Convert.ToString(version & 0xFFFF) + ":" + Convert.ToString(version >> 16);
                }






                public static void Login(Client client, Struct.Login login)
                {
                    loginGate.Pass(client);
                    Console.Out.Write(VersionToString(login.Version));
                    Console.Out.WriteLine("/" + login.Name);
                    Out.Struct.LoginResponse response = new Out.Struct.LoginResponse();
                    response.AccountID = ID.Invalid.Value;
                    response.CurrentAvatarID = ID.Invalid.Value;
                    response.TimeStamp = (UInt64)NetTime.Query();
                    if (login.Version != expectedVersion)
                    {
                        response.Success = false;
                        response.Message = "Incompatible protocol version detected (" + VersionToString(login.Version) + "). Expected version " + VersionToString(expectedVersion) + ". Please update your client.";
                        Program.LogServiceDenial(client, response.Message, login.Name);
                        Out.Channel.LoginResponse.SendTo(client, response);
                        return;
                    }
                    if (client.Account != null)
                    {
                        response.AccountID = client.Account.ID.Value;
                        response.CurrentAvatarID = client.Account.ActiveAvatar.ID.Value;
                        response.Success = false;
                        response.Message = "You are already logged in.";
                        Program.LogServiceDenial(client, response.Message, login.Name);
                        Out.Channel.LoginResponse.SendTo(client, response);
                        return;
                    }

                    AvatarInstance incarnate = null;
                    if (login.CreateAccount)
                    {
                        accountCreationGate.Pass(client);
                        if (NameIsInvalid(login.Name, out response.Message))
                            response.Success = false;
                        else
                        {
                            Account account = Account.Create(login.Name, login.Password);
                            if (account == null)
                            {
                                response.Success = false;
                                response.Message = "The specified account name is already registered. Please check the name and try again.";
                                Program.LogServiceDenial(client, response.Message, login.Name);
                            }
                            else
                            {
                                incarnate = account.CreateAvatar(PrimaryPlatform.Singleton.SpawnAvatarPosition(), false);
                                Program.Log(client, Significance.Unusual, "New account " + account + " created.");
                                Out.Channel.AvatarAppeared.Broadcast(client.Buffer, new Out.Struct.AvatarAppearance(incarnate));

                                response.Success = true;
                                response.Message = "Welcome to the Consensus Network.";
                                client.Account = account;
                                client.Account.Activate(client);
                                response.AccountID = client.Account.ID.Value;
                                response.CurrentAvatarID = incarnate.ID.Value;
                                Program.Log(client, Significance.Common, "Avatar '" + login.Name + "'/" + incarnate.ID + " created at " + incarnate.System.Position + ("(spawned on primary platform)"));

                                World.TrySave();
                            }
                        }
                    }
                    else
                    {
                        Account account = Account.Collection.FindByName(login.Name);
                        if (account == null || !account.HashPassword(client.PasswordSalt).SequenceEqual(login.Password))
                        {
                            response.Success = false;
                            response.Message = "Incorrect username or password.\nPlease check your credentials and try again.";
                            Program.LogServiceDenial(client, response.Message, login.Name);
                        }
                        else
                        {
                            response.Success = true;
                            response.Message = "";
                            client.Account = account;
                            client.Account.Activate(client);
                            incarnate = account.ActiveAvatar;
                            Debug.Assert(incarnate != null);
                            response.AccountID = client.Account.ID.Value;
                            response.CurrentAvatarID = incarnate.ID.Value;
                        }
                    }
                    Out.Channel.LoginResponse.SendTo(client, response);

                    if (response.Success || client.Account != null)
                    {
                        lock (globalChange)
                        {
                            Program.Log(client, Significance.Common, "Authenticated");


                            Structure.Collection.VisitElements(structure =>
                            {
                                SendStructureTo(client, structure);
                            });

                            #if false
                                //debug structures:
                                PrimaryPlatform[] debugPlatforms = new PrimaryPlatform[3]{PrimaryPlatform.SpawnDebugPlatform(new Vec3(40,0,100),10),
                                                                    PrimaryPlatform.SpawnDebugPlatform(new Vec3(40,40,100),10),
                                                                    PrimaryPlatform.SpawnDebugPlatform(new Vec3(0,40,100),10)};
                                Bridge.Node[] debugNodes = new Bridge.Node[] { new Bridge.Node(new Vec3(0, 10, 100), new Vec3(0,1,0),new Vec2(2,0)),
                                                                                new Bridge.Node(new Vec3(0, 30, 100), new Vec3(0,1,0),new Vec2(2,0))};
                                Bridge.Segment[] debugSegments = new Bridge.Segment[] { new Bridge.Segment(debugNodes[0],debugNodes[1])};
                                for (int i = 0; i < debugPlatforms.Length; i++)
                                {
                                    var desc = debugPlatforms[i].Description;
                                    desc.ID = (uint)(1000 + i);
                                    SendStructureTo(client, debugPlatforms[i]);
                                }
                                for (int i = 0; i < debugNodes.Length; i++)
                                {
                                    var desc = debugNodes[i].Description;
                                    desc.ID = (uint)(1000 + i + debugPlatforms.Length);
                                    SendStructureTo(client, debugNodes[i]);
                                }
                                for (int i = 0; i < debugSegments.Length; i++)
                                {
                                    var desc = debugSegments[i].Description;
                                    debugSegments[i].OverrideNodeIndices(1000 + i + debugPlatforms.Length, 1000 + i + debugPlatforms.Length + 1);
                                    desc.ID = (uint)(1000 + i + debugPlatforms.Length + debugNodes.Length);
                                    SendStructureTo(client, debugSegments[i]);
                                }
                            #endif //false



                            AvatarInstance.Collection.VisitElements(instance =>
                                {
                                    Out.Channel.AvatarAppeared.SendTo(client, new Out.Struct.AvatarAppearance(instance));
                                    Out.Channel.AvatarConditionChange.SendTo(client, new Out.Struct.AvatarConditionChange(instance));
                                });



                            if (incarnate != null)
                            {
                            
                                client.Account.SwitchTo(incarnate);
                                client.ActiveAvatar = incarnate;

                                Out.Channel.AvatarConditionChange.Broadcast(client.Buffer, new Out.Struct.AvatarConditionChange(incarnate, AvatarCondition.Active));
                                InOut.Struct.AvatarState iresponse = new InOut.Struct.AvatarState(incarnate);
                                Out.Channel.Incarnate.SendTo(client, iresponse);
                            }


                            client.StartUpdateThread();
                        }
                    }
                    Debug.Assert((client.Account != null) == (client.ActiveAvatar != null));
                    if (client.Account != null)
                        Debug.Assert(client.Account == client.ActiveAvatar.Owner);
                }

                //private static object avatarLock = new object();

                public static void CreateAvatarInstance(Client client, InOut.Struct.AvatarState state)
                {
                    lock (globalChange)
                    {
                        if (client.Account == null)
                        {
                            GeneralError(client, "You must control an avatar before you can create instances.");
                            return;
                        }
                        Account acc = client.Account;
                        lock (acc.Avatars)
                        {
                            foreach (AvatarInstance av in acc.Avatars)
                                if (av != client.ActiveAvatar)
                                {
                                    float dist = Vec.Distance(av.System.Position, state.Position);
                                    if (dist < 2.0f)
                                    {
                                        GeneralError(client, "You cannot create a new instance in such close proximity to another of your instances. The nearest other instance is "+dist+" m away. Must be at least 2 meters.");
                                        return;
                                    }
                                }
                        }
                        avatarCreationGate.Pass(client);

                        AvatarInstance newAvatar = acc.CreateAvatar(state.ToSystem(), false);

                        Out.Channel.AvatarAppeared.Broadcast(client.Buffer, new Out.Struct.AvatarAppearance(newAvatar));
                        Program.Log(client, Significance.Common, "Avatar " + newAvatar + " created at " + newAvatar.System.Position + "(spawned on user coordinates)");
                    }
                    World.TrySave();
                }

                public static void DestroyAvatarInstance(Client client, UInt32 id)
                {
                    lock (globalChange)
                    {
                        AvatarInstance av = AvatarInstance.Collection.Lookup(id);
                        Account acc = client.Account;
                        if (av == null)
                            throw new Exception("Invalid avatar instance ID ("+id+") specified for destruction.");
                        if (av.Owner != acc)
                            throw new Exception("Cannot destroy avatar instance ID (" + id + "): Instance owned by someone else: " + av.Owner);
                        if (av == client.ActiveAvatar)
                        {
                            GeneralError(client, "You cannot destroy the avatar instance you are currently using.");
                            return;
                        }

                        Program.Log(client, Significance.Common, "Erasing avatar " + av.ID + " of account " + acc + "...");
                        acc.RemoveAvatar(av);
                        AvatarInstance.Collection.Remove(av);
                        Out.Channel.AvatarVanished.Broadcast(client.Buffer, id);
                    }
                    World.TrySave();
                }

                public static void Incarnate(Client client, UInt32 id)
                {
                    incarnationGate.Pass(client);
                    lock (globalChange)
                    {
                        if (client.ActiveAvatar == null)
                        {
                            GeneralError(client, "You must first select an avatar, before you can incarnate into a different instance of that avatar.");
                            return;
                        }
                        AvatarInstance av = AvatarInstance.Collection.Lookup(id);
                        Account acc = client.Account;
                        if (av == client.ActiveAvatar)
                            return; //weird shit happens, all good though
                        if (av == null)
                            throw new Exception("Invalid avatar instance ID (" + id + ") specified for incarnation.");
                        if (av.Owner != acc)
                            throw new Exception("Invalid avatar instance ID (" + id + ") specified for incarnation. Instance belongs to someone else's avatar: " + av);
                        Out.Channel.AvatarConditionChange.Broadcast(client.Buffer, new Out.Struct.AvatarConditionChange(client.ActiveAvatar,AvatarCondition.Idle));
                        acc.SwitchTo(av);
                        client.ActiveAvatar = av;

                        Out.Channel.AvatarConditionChange.Broadcast(client.Buffer, new Out.Struct.AvatarConditionChange(av,AvatarCondition.Active));
                        InOut.Struct.AvatarState response = new InOut.Struct.AvatarState(av);
                        Out.Channel.Incarnate.SendTo(client, response);

                        Program.Log(client, Significance.Common, "Changed active instance (@ " + av.System.Position+")");
                    }
                }

                public static void OpenChannel(Client client, UInt32 channelID)
                {
                    lock (globalChange)
                    {
                        if (client.Account == null)
                        {
                            GeneralError(client, "Unable to open channel: You are not logged in");
                            return;
                        }
                        client.Account.OpenChannel(channelID);
                        Program.Log(client, Significance.Common, "Opened channel "+channelID);
                    }
                }
                public static void CloseChannel(Client client, UInt32 channelID)
                {
                    lock (globalChange)
                    {
                        if (client.Account == null)
                            return;
                        client.Account.CloseChannel(channelID);
                        Program.Log(client, Significance.Common, "Closed channel " + channelID);
                    }
                }
                

                public static void ClientMessage(Client client, Struct.ClientMessage message)
                {
                    messageGate.Pass(client);
                    lock (globalChange)
                    {
                        if (client.Account == null)
                        {
                            GeneralError(client, "Unable to relay message: you are not logged in");
                            return;
                        }
                        Out.Struct.PropagatedClientMessage response = new Out.Struct.PropagatedClientMessage() 
                        {
                            Type = message.Type,
                            Source = new Out.Struct.AccountAvatar(client.ActiveAvatar),
                            Message = message.Message,
                            ChannelID = 0
                        };

                        switch (message.Type)
                        {

                            case MessageType.ChannelMessage:
                                if (message.Target == ID.Invalid)
                                {
                                    GeneralError(client, "Unable to relay message: 0 is not a valid channel ID");
                                }
                                else
                                {
                                    response.ChannelID = message.Target;
                                    Out.Channel.ClientMessage.Broadcast(client.Buffer, response, cl => cl.IsListeningToChannel(message.Target));
                                    Program.Log(client, Significance.Common, "To channel " + message.Target + ": '" + message.Message + "'");
                                }
                            break;
                            case MessageType.WorldMessage:
                                Vec3 pos = client.ActiveAvatar.ActualPosition;

                                Out.Channel.ClientMessage.SerialInterface.SerializePacket(Out.Channel.ClientMessage.ChannelID, response, client.Buffer);
                                AvatarInstance.Collection.VisitElements(av =>
                                {
                                    lock (av)
                                    {
                                        if (av.ControlledBy != null && Vec.Distance(av.ActualPosition, pos) < 50.0f)
                                            av.ControlledBy.Transfer(client.Buffer);
                                    }
                                });
                                Program.Log(client,Significance.Common,"To world (near "+pos+"): '"+message.Message+"'");
                            break;
                            case MessageType.PrivateMessage:
                                AvatarInstance target = AvatarInstance.Collection.Lookup(message.Target);
                                if (target == null)
                                {
                                    GeneralError(client, "Unable to relay message: The specified target avatar does not exist.");
                                    return;
                                }
                                if (target.ControlledBy == null)
                                {
                                    GeneralError(client, "Unable to relay message: "+target.Owner.Name + " is currently offline.");
                                    return;
                                }
                                Program.Log(client,Significance.Common,"To "+target+"("+target.Owner+"): '"+message.Message+"'");
                                Out.Channel.ClientMessage.SendTo(client.Buffer,target.ControlledBy, response);
                                response.ChannelID = message.Target;    //this is a confirmation
                                Out.Channel.ClientMessage.SendTo(client, response);//echo
                            break;
                            default:
                                throw new Exception("Unexpected message type encountered: " + message.Type);
                        }

                    }
                }

                public static void UpdateMyInstance(Client client, InOut.Struct.DynamicAvatarState state)
                {
                    AvatarInstance activeAvatar = client.ActiveAvatar;
                    Out.Channel.ConfirmUpdateInstance.SendTo(client);
                    if (activeAvatar == null || state.ID != activeAvatar.ID)
                        return;
                    Matrix4 system = Matrix4.Assemble(InOut.DecodeDirection(state.OrientationX),InOut.DecodeDirection(state.OrientationY),state.Position);
                    if (!activeAvatar.Update(system, state.LinearVelocity, (byte)(state.FlagsAndCameraDistance >> 24), (long)state.ServerTime))
                        Program.ReportUnexpected(client, new Exception("Update delay has exceeded threshold"));
                        //throw new Exception("Update delay has exceeded threshold");

                    client.LookupBuffer.Clear();
                    Structure.BVH.Lookup(state.Position, ref client.LookupBuffer);
                    bool changed = false;
                    lock (activeAvatar.CurrentContexts)
                    {
                        if (!activeAvatar.CurrentContexts.SequenceEqual(client.LookupBuffer))
                        {
                            activeAvatar.CurrentContexts.Clear();
                            foreach (var el in client.LookupBuffer)
                                activeAvatar.CurrentContexts.Add(el);
                            changed = true;
                        }
                    }
               
                    if (changed)
                        Out.Channel.AvatarConditionChange.Broadcast(client.Buffer, new Out.Struct.AvatarConditionChange(activeAvatar, AvatarCondition.Active));
                }

                public static void NameOfAccount(Client client, UInt32 id)
                {
                    if (client.Account == null)
                        return;
                    Account acc = Account.Collection.Lookup(id);
                    if (acc != null)
                    {
                        Out.Struct.AccountName name = new Out.Struct.AccountName{ID = id, Name = acc.Name};
                        Out.Channel.AccountName.SendTo(client, name);
                    }
                }

                public static void ResetMyInstance(Client client)
                {
                    if (client.ActiveAvatar == null)
                        return;
                    lock (client.ActiveAvatar)
                    {

                        client.ActiveAvatar.SetSystem(PrimaryPlatform.Singleton.SpawnAvatarPosition());
                        client.ActiveAvatar.LinearVelocity = Vec3.Zero;
                        client.ActiveAvatar.SignalStateChange();
                        InOut.Struct.AvatarState response = new InOut.Struct.AvatarState(client.ActiveAvatar);
                        Out.Channel.Incarnate.SendTo(client, response);
                        Program.Log(client, Significance.Low, "Instance reset to " + client.ActiveAvatar.ActualPosition);
                    }
                }

                public static void InstancesUpdated(Client client, byte value)
                {
                    client.SignalUpdateConfirmation(value);
                }

                public static void StructuresUpdated(Client client, byte value)
                {
                    client.SignalStructureUpdateConfirmation(value);
                }

                public static void FileInfoRequest(Client client, UInt32 idValue)
                {
                    FileObject obj = FileObject.Collection.Lookup(idValue);
                    if (obj == null)
                    {
                        GeneralError(client, "The specified ID (" + idValue + ") does not point to any known file");
                        return;
                    }
                    Out.Struct.FileInfo info = new Out.Struct.FileInfo
                    {
                        FileID = idValue,
                        URL = null,
                        ConsensusFileInfo = new Out.Struct.ConsensusFileInfo(obj)
                    };
                    Out.Channel.FileInfo.SendTo(client, info);
                }


                public static void FileChunkRequest(Client client, In.Struct.FileChunkRequest request)
                {
                    FileObject obj = FileObject.Collection.Lookup(request.FileID);
                    if (obj == null)
                    {
                        Out.Channel.FileChunk.SendTo(client, new Out.Struct.FileChunk(request, null));
                        GeneralError(client, "The specified ID (" + request.FileID + ") does not point to any known file");
                        return;
                    }
                    if (request.ChunkIndex >= obj.NumChunks)
                    {
                        Out.Channel.FileChunk.SendTo(client, new Out.Struct.FileChunk(request,null));
                        GeneralError(client, "The requested chunk (" + request.ChunkIndex + ") of file ID " + request.FileID + " is beyond the allowed range");
                        return;
                    }
                    Out.Struct.FileChunk chunk = new Out.Struct.FileChunk(request,obj.GetChunk(request.ChunkIndex));
                    Out.Channel.FileChunk.SendTo(client, chunk);
                }

                public static void FriendRemove(Client client, UInt32 accountID)
                {
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot alter friendship until you have selected an avatar");
                        return;
                    }
                    lock (globalChange)
                    {
                        Account localAccount = client.Account;
                        Account remoteAccount = Account.Collection.Lookup(accountID);
                        if (remoteAccount == null)
                        {
                            GeneralError(client, "Cannot remove friendship. The requested account (" + accountID + ") does not exist");
                            return;
                        }
                        lock (localAccount)
                        {
                            if (!localAccount.Friends.Contains(remoteAccount))
                            {
                                return;
                            }
                            lock (remoteAccount)
                            {
                                localAccount.Friends.Remove(remoteAccount);
                                remoteAccount.Friends.Remove(localAccount);
                            }
                        }
                        if (remoteAccount.IsLoggedIn)
                        {
                            try
                            {
                                Out.Channel.FriendRemove.SendTo(client.Buffer, remoteAccount.LoggedInFrom, localAccount.ID.Value);
                            }
                            catch { }
                        }
                        Out.Channel.FriendRemove.SendTo(client, remoteAccount.ID.Value);
                    }
                    World.TrySave();
                }

                public static void FriendAccept(Client client, UInt32 accountID)
                {
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot accept or deny friendship until you have selected an avatar");
                        return;
                    }
                    Account local = client.Account;
                    if (!local.OpenFriendRequests.ContainsKey(accountID))
                    {
                        GeneralError(client, "Cannot accept friendship. The requested avatar (" + accountID + ") has not requested friendship with you");
                        return;
                    }
                    Account remote = Account.Collection.Lookup(accountID);
                    if (remote == null)
                    {
                        GeneralError(client, "Cannot accept friendship. The requested avatar (" + accountID + ") does not exist");
                        return;
                    }

                    lock (globalChange)
                    {
                        string dummy;
                        lock (local)
                            lock (remote) //global change lock should prevent deadlock here, but dual lock is required to prevent inconsistent world saving
                            {
                                int idummy;
                                local.FriendRequestDenyCounter.TryRemove(accountID, out idummy);
                                bool success = local.OpenFriendRequests.TryRemove(accountID, out dummy);
                                Debug.Assert(success);
                                if (!local.Friends.Contains(remote))
                                    local.Friends.Add(remote);
                                else
                                    Debug.Fail("Remote avatar is already registered as friend");

                                remote.FriendRequestDenyCounter.TryRemove(local.ID.Value, out idummy);
                                remote.OpenFriendRequests.TryRemove(local.ID.Value, out dummy);
                                if (!remote.Friends.Contains(local))
                                    remote.Friends.Add(local);
                                else
                                    Debug.Fail("Local avatar is already registered as friend on remote avatar");

                                Program.Log(client, Significance.Common, "Friendship accepted. Now friends with " + remote);
                            }
                        lock (remote)
                        {
                            if (remote.IsLoggedIn)
                            {
                                try
                                {
                                    Out.Channel.FriendAccept.SendTo(client.Buffer, remote.LoggedInFrom, new Out.Struct.FriendAcceptNotify(client.ActiveAvatar, true));
                                }
                                catch { }
                                Out.Channel.FriendOnline.SendTo(client, accountID);
                            }
                            else
                                Out.Channel.FriendOffline.SendTo(client, accountID);
                        }
                    }
                    World.TrySave();
                }

                public static void FriendDeny(Client client, UInt32 accountID)
                {
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot accept or deny friendship until you have selected an avatar");
                        return;
                    }
                    Account local = client.Account;
                    if (!local.OpenFriendRequests.ContainsKey(accountID))
                    {
                        Program.Log(client, Significance.Unusual, "Unrequested friend ship denied to avatar " + accountID);
                        return;
                    }

                    lock (globalChange)
                    {
                        string dummy;
                        lock (local)
                        {
                            bool success = local.OpenFriendRequests.TryRemove(accountID, out dummy);
                            Debug.Assert(success);
                            int counter;
                            if (!local.FriendRequestDenyCounter.TryGetValue(accountID, out counter))
                                counter = 0;
                            counter++;
                            local.FriendRequestDenyCounter[accountID] = counter;
                            Program.Log(client, Significance.Unusual, "Friendship denied. Deny counter with " + accountID + " increased to " + counter);

                        }
                    }
                    World.TrySave();
                }

                public static void FriendRequest(Client client, In.Struct.FriendRequest request)
                {
                    friendRequestGate.Pass(client);
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot request friendship until you have selected an avatar");
                        return;
                    }
                    lock (globalChange)
                    {
                        Account remote = Account.Collection.Lookup(request.TargetAvatar);
                        Account local = client.Account;
                        if (remote == null)
                        {
                            GeneralError(client, "Cannot request friendship. The requested avatar ("+request.TargetAvatar+") does not exist");
                            return;
                        }
                        if (remote == local)
                        {
                            GeneralError(client, "You cannot request friendship with yourself");
                            return;
                        }
                        int counter;
                        if (remote.FriendRequestDenyCounter.TryGetValue(local.ID.Value, out counter) && counter >= 3)
                        {
                            //GeneralError(client, "You have been denied by "+av.ToString()+" three times. You cannot request any more often.");
                            //silent
                            return;
                        }
                        if (remote.Friends.Contains(local))
                        {
                            Out.Channel.FriendAccept.SendTo(client, new Out.Struct.FriendAcceptNotify(remote.ActiveAvatar, remote.IsLoggedIn));
                            Program.Log(client, Significance.Unusual, "Double friend request received for " + remote.ToString());
                            return;
                        }
                        Program.Log(client, Significance.Common, "Requesting friendship with " + remote);
                        remote.OpenFriendRequests[local.ID.Value] = request.Message;
                        if (remote.IsLoggedIn)
                        {
                            try
                            {
                                Out.Channel.FriendRequest.SendTo(client.Buffer, remote.LoggedInFrom, new Out.Struct.PropagatedFriendRequest(local.ActiveAvatar, request.Message));
                                return;
                            }
                            catch
                            { }
                        }
                    }
                    World.TrySave();
                }

                public static void CreateStructure(Client client, In.Struct.CreateStructure structure)
                {
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot create structures until you have selected an avatar");
                        return;
                    }

                    structureCreationGate.Pass(client);
                    lock (globalChange)
                    {
                        Structure strct = Structure.Load(structure.Contract,structure.Structure);
                        SendStructureToAll(strct);
                        if (strct.Realtime != null)
                            strct.Realtime.Set(structure.State.ToSystem(),structure.State.LinearVelocity,structure.State.Momentum,structure.State.FlagsAndCameraDistance,NetTime.Query());

                        if (structure.Board)
                            client.ActiveAvatar.Board(strct,client.Buffer);
                    }
                }

                public static void EnterStructure(Client client, UInt32 structureID)
                {
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot enter structures until you have selected an avatar");
                        return;
                    }
                    structureBoardGate.Pass(client);
                    lock(globalChange)
                    {
                        Structure structure = Structure.Collection.Lookup(structureID);
                        if (structure == null)
                        {
                            GeneralError(client, "Unable to board unknown structure");
                            return;
                        }
                        client.ActiveAvatar.Board(structure,client.Buffer);
                    }
                }
                
                public static void ExitStructure(Client client)
                {
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot enter structures until you have selected an avatar");
                        return;
                    }
                    structureBoardGate.Pass(client);
                   // lock(globalChange)
                    {
                        client.ActiveAvatar.Board(null,client.Buffer);
                    }

                }

                public static void DestroyStructure(Client client, UInt32 structureID)
                {
                    if (client.ActiveAvatar == null)
                    {
                        GeneralError(client, "You cannot destroy structures until you have selected an avatar");
                        return;
                    }
                    structureDestructionGate.Pass(client);
                    lock(globalChange)
                    {
                        Structure structure = Structure.Collection.Lookup(structureID);
                        if (structure == null)
                        {
                            GeneralError(client, "Unable to destroy unknown structure");
                            return;
                        }
                        if (structure.IsOccupied)
                        {
                            GeneralError(client, "Unable to destroy structure "+structure.ID+". This structure is still occupied by at least one avatar instance");
                            return;
                        }
                        structure.Dispose();
                        Out.Channel.StructureVanished.Broadcast(client.Buffer,structureID);
                    }
                }
            }
        }
    }



    public class StateFrame
    {
        private static SerialInterface serializer = SerialInterface.Build(typeof(Interface.Out.Struct.InstanceFrame));
        private Dictionary<ID, int> lastVersion = new Dictionary<ID, int>();
        private List<Interface.InOut.Struct.DynamicAvatarState>    result = new List<Interface.InOut.Struct.DynamicAvatarState>();

        public void BuildFullFrame(ByteBuffer buffer, byte serial)
        {
            result.Clear();

            AvatarInstance.Collection.VisitElements(instance =>
            {
                lock (instance)
                {
                    Interface.InOut.Struct.DynamicAvatarState state = new Interface.InOut.Struct.DynamicAvatarState(instance);
                    Debug.Assert(instance.ControlledBy != null || instance.LinearVelocity.Length == 0);
                    lastVersion[instance.ID] = instance.GetVersion();
                    result.Add(state);
                }
            });
            serializer.SerializePacket(
                (UInt32)Interface.ChannelID.AvatarsUpdated,
                new Interface.Out.Struct.InstanceFrame
                {
                    Serial = serial, 
                    InstanceList = result.ToArray()
                },
                buffer);
        }

        public void BuildDeltaFrame(ByteBuffer buffer, byte serial)
        {
            result.Clear();
            int version;

            AvatarInstance.Collection.VisitElements(avatar =>
            {
                //AvatarInstance instance = avatar.ActiveAvata;
                lock (avatar)
                {
                    if (!lastVersion.TryGetValue(avatar.ID, out version) || avatar.IsOutdated(version))
                    {
                        Interface.InOut.Struct.DynamicAvatarState state = new Interface.InOut.Struct.DynamicAvatarState(avatar);
                        Debug.Assert(avatar.ControlledBy != null || avatar.LinearVelocity.Length == 0);
                        result.Add(state);
                        lastVersion[avatar.ID] = avatar.GetVersion();
                    }
                }
            });
            if (result.Count > 0)
                serializer.SerializePacket(
                    (UInt32)Interface.ChannelID.AvatarsUpdated,
                    new Interface.Out.Struct.InstanceFrame
                    { 
                        Serial = serial, 
                        InstanceList = result.ToArray()
                    }, 
                    buffer);
            else
                buffer.Clear();
        }
    }


    public class StructureStateFrame
    {
        private static SerialInterface serializer = SerialInterface.Build(typeof(Interface.Out.Struct.StructureFrame));
        private Dictionary<ID, int> lastVersion = new Dictionary<ID, int>();
        private List<Interface.InOut.Struct.StructureState> result = new List<Interface.InOut.Struct.StructureState>();

        public void BuildFullFrame(ByteBuffer buffer, byte serial)
        {
            result.Clear();

            Structure.RealtimeStructures.VisitElements(structure =>
            {
                Structure.RealtimeControl ctrl = structure.Realtime;
                ctrl.Lock();
                {
                    Interface.InOut.Struct.StructureState state = new Interface.InOut.Struct.StructureState(ctrl);
                    lastVersion[structure.ID] = ctrl.GetVersionNoLock();
                    result.Add(state);
                }
                ctrl.Release();
            });
            serializer.SerializePacket(
                (UInt32)Interface.ChannelID.StructuresUpdated,
                new Interface.Out.Struct.StructureFrame
                {
                    Serial = serial,
                    StructureList = result.ToArray()
                },
                buffer);
        }

        public void BuildDeltaFrame(ByteBuffer buffer, byte serial)
        {
            result.Clear();
            int version;

            Structure.RealtimeStructures.VisitElements(structure =>
            {
                Structure.RealtimeControl ctrl = structure.Realtime;
                ctrl.Lock();
                {
                    if (!lastVersion.TryGetValue(structure.ID, out version) || ctrl.IsOutdatedNoLock(version))
                    {
                        Interface.InOut.Struct.StructureState state = new Interface.InOut.Struct.StructureState(ctrl);
                        result.Add(state);
                        lastVersion[structure.ID] = structure.Realtime.GetVersionNoLock();
                    }
                }
                ctrl.Release();
            });
            if (result.Count > 0)
                serializer.SerializePacket(
                    (UInt32)Interface.ChannelID.StructuresUpdated,
                    new Interface.Out.Struct.StructureFrame
                    {
                        Serial = serial,
                        StructureList = result.ToArray()
                    },
                    buffer);
            else
                buffer.Clear();
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
        public static string ToString(ID v)
        {
            return v.Value.ToString(culture);
        }

    }



    public enum Significance
    {
        Low,
        Common,
        Unusual,
        Important,
        ClientFatal,
        ProgramFatal
    }

    class Program
    {
        public const int Port = (int)Interface.Port.Main,
                        TimeServerPort = (int)Interface.Port.Time,
                        ViewServerPort = (int)Interface.Port.View;

        static TcpListener  listener;
        static Thread listenThread;
        static Logger logger = new Logger();

        public static volatile bool ShuttingDown = false;


        public static void ShutDown()
        {
            ShuttingDown = true;
            if (listener != null)
                listener.Stop();
        }

        public static void ReportUnexpected(Client origin, Exception ex, Significance significance = Significance.ClientFatal)
        {
            //Debug.Fail(ex.ToString());
            Log(origin, significance, ex.ToString());
            //more later
        }
        public static void ReportCommon(Client origin, Exception ex)
        {
            //Debug.Fail(ex.ToString());
            Log(origin, Significance.Low, ex.ToString());
            //more later
        }

        public static void ReportFatal(Client origin, Exception ex)
        {
            ReportUnexpected(origin, ex, Significance.ProgramFatal);
            ShutDown();
        }

        public static void LogServiceDenial(Client requester, String errorMessage, object additionalParameter = null)
        {
            errorMessage = "Client request denied: '"+errorMessage+"'";
            if (additionalParameter != null)
                errorMessage += " (with parameter='" + additionalParameter + "')";
            Log(requester, Significance.Unusual, errorMessage);
        }


        private static object logLock = new object();

        public static void Log(Client origin, Significance significance, String message)
        {
            string printLine,fileLine;
            fileLine = DateTime.Now.ToString();
            fileLine += "\t" + significance;

            if (origin != null)
            {
                fileLine += "\t" + origin;

                Account ac = origin.Account;
                if (ac != null)
                {
                    fileLine += "\t" + ac.ID + "\t" + ac.Name;

                    AvatarInstance av = origin.ActiveAvatar;
                    if (av == null)
                    {
                        fileLine += "\t" + "\t" + "\t";
                        printLine = System.String.Format("[{1}]<{0}> {2}({3}): {4}", significance, DateTime.Now, origin, ac, message);
                    }
                    else
                    {
                        fileLine += "\t" + av.ID + "\t" + "\t";
                        printLine = System.String.Format("[{1}]<{0}> {2}({3} as {5}): {4}", significance, DateTime.Now, origin, ac, message, av);
                    }
                }
                else
                {
                    fileLine += "\t" + "\t";
                    fileLine += "\t" + "\t" + "\t";

                    printLine = System.String.Format("[{1}]<{0}> {2}: {3}", significance, DateTime.Now, origin, message);
                }
            }
            else
            {
                fileLine += "\t";
                fileLine += "\t" + "\t";
                fileLine += "\t" + "\t" + "\t";

                printLine = System.String.Format("[{1}]<{0}> {2}", significance, DateTime.Now, message);
            }

            fileLine += "\t" + message;

            logger.SendEvent(fileLine);

            if (significance >= Significance.Unusual)
            {
                lock (logLock)
                    System.IO.File.AppendAllText("execution.log", fileLine + "\n");
                Console.WriteLine(printLine);
            }
        }


        static void Listen()
        {
            // Is someone trying to call us? Well answer!
            Log(null,Significance.Low,"Starting listening thread on "+listener.LocalEndpoint.ToString());
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
                    ReportUnexpected(null, ex,Significance.Unusual);
                }
            }
            Log(null, Significance.Important, "Closing down listening thread");
        }

        private static volatile bool didEnd = false;

        static void end()
        {
            listenThread.Join();
            TimeServer.End();
            ViewServer.End();
            
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

                    World.Save();

                    foreach (ReportProvider p in ReportProvider.All)
                    {
                        p.Close();
                    }
                }
            }
        }

        static void onCancel(object sender, ConsoleCancelEventArgs e)
        {
            ShutDown();
            end();
        }



        static void Main(string[] args)
        {
            if (System.IO.Directory.Exists("./Runtime"))
                System.IO.Directory.SetCurrentDirectory("./Runtime");
            else
                System.IO.Directory.SetCurrentDirectory("../../Runtime");   //drop down from build-directory


            Console.CancelKeyPress += new ConsoleCancelEventHandler(onCancel);

            {
                XElement xconfig = XElement.Load("config.xml");

                string smode = xconfig.GetString("server_mode");
                ProgramMode mode = (ProgramMode)Enum.Parse(typeof(ProgramMode), smode, true);
                ViewServer.Init(xconfig, mode);
            }
            


            Interface.In.Register();

            if (!World.Load())
                return;

            Client.InitCertificate();


            //listener = new TcpListener(System.Net.IPAddress.IPv6Any, Port);   //IPv6
            listener = new TcpListener(System.Net.IPAddress.Any, Port); //IPv4
            listener.Start();

            listenThread = new Thread(new ThreadStart(Listen));
            listenThread.Start();

            TimeServer.Start(TimeServerPort);
            ViewServer.Start(ViewServerPort);

            logger.Start();

            System.Threading.Thread.Sleep(100);

            //World.Save();

            Log(null, Significance.Important, "Initialization phase completed. Awaiting clients.");


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
            array[address+1] = (byte)((v >> 8) & 0xFF);
            array[address+2] = (byte)((v >> 16) & 0xFF);
            array[address+3] = (byte)((v >> 24) & 0xFF);
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
            element.SetAttributeValue(key, Convert.ToString(v.x)+","+Convert.ToString(v.y)+","+Convert.ToString(v.z));
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
        public static void Set(this XElement element, string key, ID v)
        {
            element.SetAttributeValue(key, Convert.ToString(v));
        }

        public static float GetFloat(this XElement element, string key)
        {
            return Convert.ToFloat(element.Attribute(key).Value);
        }
        public static Vec3 GetVec3(this XElement element, string key)
        {
            string val = element.Attribute(key).Value;
            string[] elements = val.Split(new char[] { ',' });
            if (elements.Length != 3)
                throw new Exception("Expected 3 elements while parsing xml element '" + key + "' (value is '"+val+"'), but got "+elements.Length);
            return new Vec3(Convert.ToFloat(elements[0].Trim()),
                            Convert.ToFloat(elements[1].Trim()),
                            Convert.ToFloat(elements[2].Trim()));
        }
        public static Vec4 GetVec4(this XElement element, string key)
        {
            string val = element.Attribute(key).Value;
            string[] elements = val.Split(new char[] { ',' });
            if (elements.Length != 4)
                throw new Exception("Expected 4 elements while parsing xml element '" + key + "' (value is '" + val + "'), but got " + elements.Length);
            return new Vec4(Convert.ToFloat(elements[0].Trim()),
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




        public static bool SimilarTo(this float value, float otherValue)
        {
            return Math.Abs(value - otherValue) <= 1e-6f;
        }
        public static bool SimilarTo(this float value, float otherValue, float maxError)
        {
            return Math.Abs(value - otherValue) <= maxError;
        }

    }
}
