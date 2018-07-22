using System;
using System.Net.Sockets;
using System.Threading;

namespace ServerManager
{
	public class LocalhostClient : Protocol.Client
	{

		private readonly int port;

		public LocalhostClient(int port) : base(null, false)
		{
			this.port = port;
		}

		private Thread readThread;

		public void TryToReconnect()
		{
			if (IsDisposed)
				throw new ObjectDisposedException("This client is disposed");
			if (readThread != null && readThread.IsAlive)
				return;
			readThread = new Thread(new ThreadStart(ConnectAndRead));
			readThread.Start();
		}
		/// <summary>
		/// Set true if a connection is established and 
		/// </summary>
		public bool IsReady { get; private set; } = false;
		public bool IsAttemptingConnection => IsConnected && !IsReady;

		private void ConnectAndRead()
		{
			try
			{
				Reset(new TcpClient("localhost", port));
				if (IsDisposed)
					return;
				IsReady = true;
				StartNoSSLSelf();
			}
			catch (SocketException)
			{ }
			catch (Exception ex)
			{
				OnAbnormalException(ex);
			}
			IsReady = false;
		}

	}
}