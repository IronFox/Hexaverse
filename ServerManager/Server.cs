using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ServerManager
{
	public class Server : ListViewItem, IDisposable
	{
		private Process controlConsole=null, server=null;
		private readonly ServerInfoReceiver receiver;
		private readonly ControlConnection controlConnection;
		public UUID ID { get; private set; }
		public bool SessionIsClosed { get; private set; }
		public bool RequiresHeartbeats { get; private set; }
		public readonly int BasePort;
		public DateTime LastUDPPing { get; private set; }


		public Server(int basePort, Action<string> asyncLog, Action<string> asyncDisplayError)
		{
			BasePort = basePort;
			base.Text = basePort + ".." + (basePort + 2);
			//basePort + "," + (basePort + 1) + "(," + (basePort + 2) + ")";
			base.SubItems.Add("");  //server id
			base.SubItems.Add("");  //world id
			base.SubItems.Add("Unknown");  //status
			base.SubItems.Add("");  //rules

			receiver = new ServerInfoReceiver(basePort);
			controlConnection = new ControlConnection(basePort+2,(msg) =>
			{
				asyncLog(ID.ToNiceString() + ": " + msg);
			},
			(msg) =>
			{
				asyncDisplayError(ID.ToNiceString() + ": " + msg);
			});
		}

		public void Receive(UDPReceiver.Info info)
		{
			if (this.disposedValue)
				throw new ObjectDisposedException("This Server has been disposed. Cannot update");
			ID = info.HostID;
			LastUDPPing = info.Received;
			SessionIsClosed = info.SessionClosed;
			RequiresHeartbeats = info.RequiresHeartbeats;
		}

		public bool Update()
		{
			if (this.disposedValue)
				throw new ObjectDisposedException("This Server has been disposed. Cannot update");
			if (!receiver.IsConnected && !SessionIsClosed)
				receiver.TryToReconnect();
			if (!controlConnection.IsConnected)
				controlConnection.TryToReconnect();

			base.SubItems[1].Text = ID.ToNiceString();
			var status = base.SubItems[3];
			var worldID = base.SubItems[2];
			var rules = base.SubItems[4];
			base.Text = BasePort + ".." + (BasePort + 2);
			if (RequiresHeartbeats)
				base.Text += " (H)";

			bool hasInfo = receiver.IsConnected && receiver.LastUpdate != null && DateTime.Now - receiver.LastUpdate.Received < TimeSpan.FromSeconds(2);

			if (!SessionIsClosed && hasInfo)
			{
				var info = receiver.LastUpdate.Package.game;
				status.Text = info.numFactionsOnline + "/" + info.numFactionsCreated + " online, round "+info.roundNumber;
				worldID.Text = info.worldID.ToNiceString();
				rules.Text = ServerInfoReceiver.GameFlagsToString(info.gameConfig.gameFlags);
			}
			else
			{
				if (DateTime.Now - LastUDPPing > TimeSpan.FromSeconds(5))
					return false;
				if (SessionIsClosed)
				{
					worldID.Text = "";
					rules.Text = "";
					if (hasInfo)
					{
						var info = receiver.LastUpdate.Package.game;
						worldID.Text = info.worldID.ToNiceString();
						rules.Text = ServerInfoReceiver.GameFlagsToString(info.gameConfig.gameFlags);
					}
					status.Text = "Session closed or suspended";
				}
				else
				{
					worldID.Text = "?";
					rules.Text = "?";
					if (DateTime.Now - LastUDPPing > TimeSpan.FromSeconds(2))
						status.Text = "No contact";
					else
						status.Text = "Connecting...";
				}
			}


			return true;
		}

		public void IssueControlCommand(string cmd)
		{
			controlConnection.ExecuteCommand(cmd);
		}

		public void FetchVariableContentAsync(string path, Action<string> result)
		{
			controlConnection.GetValue(path, result);
		}

		public void Shutdown()
		{
			IssueControlCommand("/Server/shutdown");
		}

		[DllImport("user32.dll", SetLastError = true)]
		static extern bool SetForegroundWindow(IntPtr hWnd);

		internal void StartControlConsole()
		{
			if (controlConsole != null && !controlConsole.HasExited)
			{
				SetForegroundWindow(controlConsole.MainWindowHandle);
				return;
			}
			try
			{
				if (controlConsole != null)
					controlConsole.Dispose();
				controlConsole = null;
				controlConsole = Program.StartComponent("ControlConsole", "ControlConsole", "-p " + BasePort);
			}
			catch (Exception ex)
			{
				MessageBox.Show(ex.Message, "Error opening control console", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
			}
		}

		internal void OpenLogFile()
		{
			FetchVariableContentAsync("/IO/logFile",(logAt) =>
			{
				using (Process.Start(logAt))
				{ }
			});
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
					if (receiver != null)
						receiver.Dispose();
					if (controlConnection != null)
						controlConnection.Dispose();
					//if (server != null)	//it'll stop on its own
					//server.Kill();
					if (controlConsole != null)
					{
						if (!controlConsole.HasExited)
							controlConsole.Kill();
						controlConsole.Dispose();
					}
				}

				// TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
				// TODO: set large fields to null.

				disposedValue = true;
			}
		}

		// TODO: override a finalizer only if Dispose(bool disposing) above has code to free unmanaged resources.
		// ~Server() {
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

		internal void SetSession(bool active)
		{
			IssueControlCommand("/Server/Online/session = " + active);
		}
		#endregion
	}
}
