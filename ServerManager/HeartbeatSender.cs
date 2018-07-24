using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace ServerManager
{
	public class HeartbeatSender : IDisposable
	{
		public readonly int TargetPort;
		private UdpClient client;
		private Thread thread;

		public HeartbeatSender(int targetPort)
		{
			this.TargetPort = targetPort;

			client = new UdpClient();
			thread = new Thread(new ThreadStart(Beat));
			thread.Start();
			//readThread.Start();
		}

		private void Beat()
		{
			IPEndPoint dest = new IPEndPoint(IPAddress.Loopback, TargetPort);
			byte[] package = new byte[1];
			package[0] = 32;

			int at = 0;
			try
			{
				while (client != null)
				{
					Thread.Sleep(50);  //higher frequency for quicker stops
					if (++at < (1000 / 50))
						continue;
					at = 0;
					var cp = client;
					if (cp == null)
						return;
					cp.Send(package, 1, dest);
				}
			}
			catch
			{

			}
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
					client.Close();
					var cp = client;
					client = null;
					thread.Join();
					cp.Dispose();
					thread = null;
				}

				// TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
				// TODO: set large fields to null.

				disposedValue = true;
			}
		}

		// TODO: override a finalizer only if Dispose(bool disposing) above has code to free unmanaged resources.
		// ~HeartbeatSender() {
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