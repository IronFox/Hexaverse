using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Security;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Protocol
{
	public class Client : IDisposable
	{

		public ByteBuffer Buffer = new ByteBuffer(256);

		private TcpClient tcpClient;
		private Stream ioStream;

		public virtual ChannelMap ChannelMap { get; set; } = new ChannelMap();


		const int SafePacketSize = 64000000;

		public readonly bool RequireSSL;


		private Thread threadHandle;

		protected TcpClient TcpClient { get { return tcpClient; } }


		protected void StartSSL(SslStream stream)
		{
			if (!stream.IsEncrypted)
				throw new Exception("Client.Start(): Must be encrypted at this point");
			ioStream = stream;

			Debug.Assert(RequireSSL);
			OnReadThreadStart();
			threadHandle = new Thread(new ThreadStart(this.ThreadMain));
			threadHandle.Start();
		}

		protected void StartNoSSL()
		{
			ioStream = tcpClient.GetStream();

			Debug.Assert(!RequireSSL);
			OnReadThreadStart();
			threadHandle = new Thread(new ThreadStart(this.ThreadMain));
			threadHandle.Start();

		}
		protected void StartNoSSLSelf()
		{
			ioStream = tcpClient.GetStream();

			Debug.Assert(!RequireSSL);
			OnReadThreadStart();
			ThreadMain();
		}

		private void ThreadMain()
		{
			UInt32 channel = 0, packetSize = 0;
			ChannelHandler handler = null;

			bool readingHeader = true;

			try
			{
				byte[] buffer = new byte[tcpClient.ReceiveBufferSize],
						decodeBuffer = new byte[tcpClient.ReceiveBufferSize];
				DataQueue queue = new DataQueue(tcpClient.ReceiveBufferSize);

				while (tcpClient != null)
				{
					int read = ioStream.Read(buffer, 0, tcpClient.ReceiveBufferSize);
					if (read <= 0)
						break;
					queue.Append(buffer, read);

					do
					{
						if (readingHeader)
						{
							if (queue.Length >= 8)
							{
								channel = queue.PopUInt32();
								packetSize = queue.PopUInt32();
								readingHeader = false;
								if (packetSize > SafePacketSize)
								{
									throw new Exception("Received unsafe packet size (" + packetSize + ") on channel " + channel);
								}
								handler = ChannelMap.LookUp(channel);
								if (handler == null)
								{
									throw new Exception("Received packet on unexpected channel " + channel);
								}
								if (handler.LoggedInOnly && !IsAuthenticated)
								{
									throw new Exception("Received packet on channel " + channel + ", which is reserved for authenticated clients");
								}
							}
						}

						if (!readingHeader)
							if (queue.Length >= packetSize)
							{
								if (decodeBuffer.Length < packetSize)
									decodeBuffer = new byte[packetSize];
								queue.PopData(decodeBuffer, (int)packetSize);
								handler.Handle(this, decodeBuffer, (int)packetSize);
								readingHeader = true;
								handler = null;
							}
					}
					while (readingHeader && queue.Length >= 8);
				}
			}
			catch (ObjectDisposedException) //socket has been closed
			{ }
			catch (SocketException) //probably safe, tpp
			{ }
			catch (BlockException exception)
			{
				OnAbnormalException(exception);
			}
			catch (IOException) //this is fired if the ssl connection is terminated
			{ }
			catch (Exception ex)    //should not happen
			{
				OnAbnormalException(ex);
			}
			Close();
			OnReadThreadExit();

		}

		protected virtual void OnAbnormalException(Exception e)
		{ }

		protected virtual void OnReadThreadStart()
		{ }

		protected virtual void OnReadThreadExit()
		{ }

		protected virtual bool IsAuthenticated { get { return true; } }



		protected Client(TcpClient TCPClient, bool requireSSL)
		{
			RequireSSL = requireSSL;
			tcpClient = TCPClient;
			if (tcpClient != null)
				tcpClient.NoDelay = true;
		}

		public bool IsConnected
		{
			get
			{
				var cl = tcpClient;
				return cl != null && cl.Connected;
			}
		}

		protected void Reset(TcpClient TCPClient)
		{
			if (IsConnected)
				ForceDisconnect();
			tcpClient = TCPClient;
			tcpClient.NoDelay = true;
		}

		protected virtual void OnClose()
		{


		}

		protected void Close()
		{
			lock (this)
			{
				if (tcpClient != null)
				{
					ioStream.Close();
					ioStream.Dispose();
					ioStream = null;
					tcpClient.Close();
					tcpClient = null;
					OnClose();
				}
			}
		}

		public void Transfer(ByteBuffer data)
		{
			if (ioStream == null)
				return;
			try
			{
				lock (ioStream)
				{
					ioStream.Write(data.GetArray(), 0, data.Length);
					ioStream.Flush();
					return;
				}
			}
			catch (IOException)
			{ }
			catch (SocketException)
			{ }
			catch (ObjectDisposedException)
			{ }
			catch (Exception ex)
			{
				OnAbnormalException(ex);
			}
			Close();
		}


		protected void Join()
		{
			if (threadHandle != null)
				threadHandle.Join();
		}

		public void ForceDisconnect()
		{
			Close();
			Join();
		}

		protected bool IsDisposed => disposedValue;

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
					ForceDisconnect();
				}

				// TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
				// TODO: set large fields to null.

				disposedValue = true;
			}
		}

		// TODO: override a finalizer only if Dispose(bool disposing) above has code to free unmanaged resources.
		// ~Client() {
		//   // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
		//   Dispose(false);
		// }

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
			// TODO: uncomment the following line if the finalizer is overridden above.
			// GC.SuppressFinalize(this);
		}
		#endregion



	}
}
