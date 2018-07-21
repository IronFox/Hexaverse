using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace ServerManager
{
	public struct UUID
	{
		public readonly UInt64 First, Last;

		public UUID(byte[] source, int offset)
		{
			First = BitConverter.ToUInt64(source, offset);
			Last = BitConverter.ToUInt64(source, offset+8);
		}

		public override bool Equals(object obj)
		{
			if (!(obj is UUID))
			{
				return false;
			}

			var uUID = (UUID)obj;
			return First == uUID.First &&
				   Last == uUID.Last;
		}

		public override int GetHashCode()
		{
			var hashCode = 2082053542;
			hashCode = hashCode * -1521134295 + First.GetHashCode();
			hashCode = hashCode * -1521134295 + Last.GetHashCode();
			return hashCode;
		}

		public bool IsEmpty => First == 0 && Last == 0;



		public string ToNiceString()
		{
			return NiceName.Get(First);
		}

		public override string ToString()
		{
			return ((UInt32)First).ToString("X");
		}
		public string ToFullString()
		{
			return First.ToString("X") + Last.ToString("X");
		}
	};

	public class UDPReceiver
	{
		public class Info
		{
			public readonly UUID HostID;
			public readonly string Version;
			public readonly int Port;
			public readonly DateTime Received;

			public Info(UUID host, string version, int port)
			{
				HostID = host;
				Version = version;
				Port = port;
				Received = DateTime.Now;
			}
		};


		public const int ReceiverPort = 10247;
		private UdpClient client;
		private Thread readThread;
		private volatile bool end = false;
		private Exception readEndEx;

		public readonly ConcurrentQueue<Info> Updates = new ConcurrentQueue<Info>();

		public UDPReceiver()
		{
			client = new UdpClient(ReceiverPort);
			readThread = new Thread(new ThreadStart(Read));
			readThread.Start();
		}


		private void Read()
		{
			//Creates an IPEndPoint to record the IP Address and port number of the sender. 
			// The IPEndPoint will allow you to read datagrams sent from any source.
			IPEndPoint RemoteIpEndPoint = new IPEndPoint(IPAddress.Loopback, 0);
			try
			{
				while (!end)
				{
					Byte[] bytes = client.Receive(ref RemoteIpEndPoint);

					if (bytes.Length != 16 + 64 + 2)
						throw new IOException("Received unexpected package size "+bytes.Length);

					/*
					TUniqueID hostID;
					char version[64];
					UINT16 serverBasePort;
					*/
					Updates.Enqueue(new Info(new UUID(bytes, 0),BitConverter.ToString(bytes, 16, 64),BitConverter.ToUInt16(bytes, 80)));
				}
			}
			catch (Exception ex)
			{
				readEndEx = ex;
			}
		}





	}
}
