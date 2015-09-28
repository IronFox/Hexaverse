using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Protocol
{

   

    public enum ChannelID
    {
		Hello,			//c2s: protocol version: UINT32
		Authenticated,	//s2c: AccountDetails
		NewAccount,		//c2s->s2c: string->NewAccount

		//channels for technite control communication
		TechniteStateChunk,		//s2c: TechniteStateChunk
		InstructTechnites,		//s2c: <signal>
		TechniteInstructionChunk, //c2s: TechniteInstructionChunk

		SessionBegin,   //s2c: SessionBegin
		NodeChunk,		//s2c: NodeChunk

		Count     //must remain last
	};

    public enum Protocol
    {
        Version = 10	//must be incremented whenever the type of data being transfered along any of the above channels changes.
    }

    public enum Port
    {
        Main = 12874,
        Logger = 12879,
        Time = 12880,
    }


	public static class Version
	{
		public const UInt32 Combined = ((UInt32)ChannelID.Count << 16) | (UInt16)Protocol.Version;
	}


	namespace Interface
	{
		public struct AccountDetails
		{
			public string accountName;
			public byte[] guid;

			public AccountDetails(Account acc)
			{
				accountName = acc.Name;
				guid = acc.GUID.ToByteArray();
			}
		}
		public struct NewAccount
		{
			public AccountDetails details;
			public byte[] certificate;
		}

	}

	




	public abstract class ChannelHandler
	{
		private SerialInterface processor;
		public bool LoggedInOnly { get; private set; }


		virtual protected void OnReceive(Client sender, Object received) { throw new Exception("Expected signal but received object on channel " + ChannelID); }
		virtual protected void OnReceiveSignal(Client sender) { throw new Exception("Expected object but received signal on channel " + ChannelID); }

		public ChannelID ChannelID { get; private set; }

		public ChannelHandler(ChannelID myChannel, Type Serializable, bool loggedInOnly)
		{
			LoggedInOnly = loggedInOnly;
			ChannelID = myChannel;
			if (Serializable != typeof(void))
				processor = SerialInterface.Build(Serializable);
		}


		public void Handle(Client client, byte[] data, int dataSize)
		{
			if (dataSize == 0)
			{
				OnReceiveSignal(client);
				return;
			}
			if (processor == null)
				throw new Exception("Expected signal but received object on channel " + ChannelID);
			Object item = processor.Deserialize(data, dataSize);
			OnReceive(client, item);
		}
	}



	class GenericChannelHandler<T> : ChannelHandler
	{
		private Action<Client, T> handler;

		override protected void OnReceive(Client sender, Object received)
		{
			handler(sender, (T)received);
		}

		public GenericChannelHandler(ChannelID id, Action<Client, T> action, bool loggedInOnly)
			: base(id, typeof(T), loggedInOnly)
		{
			handler = action;
		}


	};

	class SignalHandler : ChannelHandler
	{
		private Action<Client> handler;
		override protected void OnReceiveSignal(Client sender)
		{
			handler(sender);
		}
		public SignalHandler(ChannelID id, Action<Client> action, bool loggedInOnly)
			: base(id, typeof(void), loggedInOnly)
		{
			handler = action;
		}
	}



	public class ChannelSender
	{
		private SerialInterface processor;

		public SerialInterface Processor { get { return processor;} }

		public ChannelSender(Type t)
		{
			processor = SerialInterface.Build(t);
		}


		public SerialInterface SerialInterface { get { return processor; } }

		public void SendTo(UInt32 channelID, ByteBuffer buffer, Client client, Object item)
		{
			processor.SerializePacket(channelID, item, buffer);

			client.Transfer(buffer);
			buffer.Clear();
		}

	}




	public class OutChannel<T> : ChannelSender
	{
		private UInt32 channelID;
		public OutChannel(ChannelID channelID)
			: base(typeof(T))
		{ this.channelID = (UInt32)channelID; }
		public void SendTo(Client client, T item)
		{
			base.SendTo((UInt32)channelID, client.Buffer, client, item);
		}
		public void SendTo(ByteBuffer useBuffer, Client client, T item)
		{
			base.SendTo((UInt32)channelID, useBuffer, client, item);
		}


		public UInt32 ChannelID { get { return channelID; } }
	};

	public class SignalChannel
	{
		private UInt32 channelID;
		public SignalChannel(ChannelID channelID)
		{ this.channelID = (UInt32)channelID; }
		public void SendTo(Client client)
		{
			SendTo(client.Buffer, client);
		}
		public void SendTo(ByteBuffer useBuffer, Client client)
		{
			useBuffer.Clear();
			useBuffer.AppendU32((UInt32)channelID);
			useBuffer.AppendU32(0);
			client.Transfer(useBuffer);
		}


		public UInt32 ChannelID { get { return channelID; } }
	};

	public static class StaticSender<T>
	{
		private static ChannelSender sender = new ChannelSender(typeof(T));

		public static void SendTo(ChannelID channelID, Client client, ByteBuffer buffer, T item)
		{
			sender.SendTo((UInt32)channelID, buffer, client, item);
		}
	}



	public static class ChannelMap
	{
		private static ConcurrentDictionary<ChannelID, ChannelHandler> channels = new ConcurrentDictionary<ChannelID, ChannelHandler>();

		public static ChannelHandler LookUp(ChannelID channelID)
		{
			if (!channels.ContainsKey(channelID))
				return null;
			return channels[channelID];
		}

		public static void Register<T>(ChannelID channelID, Action<Client, T> OnReceive, bool requireLogIn = true)
		{
			ChannelHandler channel = new GenericChannelHandler<T>(channelID, OnReceive, requireLogIn);
			channels[channelID] = channel;
		}

		public static void RegisterSignal(ChannelID channelID, Action<Client> onReceive, bool requireLogIn = true)
		{
			ChannelHandler channel = new SignalHandler(channelID, onReceive, requireLogIn);
			channels[channelID] = channel;
		}
	}



}
