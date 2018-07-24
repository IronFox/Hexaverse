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
		public struct Config
		{
			public int port;
			public bool isDependent;
			public string configPath;	//ignored if saveGamePath is non-null and non-empty
			public string saveGamePath; //superseeds configPath
			public bool startSession;
		};


		private Process controlConsole=null, server=null;
		private readonly ServerInfoReceiver receiver;
		private readonly ControlConnection controlConnection;
		public UUID ID { get; private set; }
		public bool SessionIsClosed { get; private set; }
		public bool RequiresHeartbeats { get; private set; }
		public readonly int BasePort;
		public const int NumPorts = 3;

		public DateTime LastUDPPing { get; private set; }

		private volatile bool aiIsEnabled = false;
		private volatile string aiScript = null;
		private HeartbeatSender heartbeatSender;

		public Server(int basePort, Action<string> asyncLog, Action<string> asyncDisplayError)
		{
			BasePort = basePort;
			base.Text = basePort + ".." + (basePort + NumPorts-1);
			base.SubItems.Add("");  //server id
			base.SubItems.Add("");  //world id
			base.SubItems.Add("No");  //ai
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

			controlConnection.GetValue("/Rules/Nexus/enableAI", (str) => { bool.TryParse(str, out aiIsEnabled); });
			controlConnection.GetValue("/Rules/Nexus/aiScript", (str) => { aiScript = str; });
		}

		public Server(Config cfg, Action<string> asyncLog, Action<string> asyncDisplayError) : this(cfg.port, asyncLog, asyncDisplayError)
		{
			SessionIsClosed = true;

			string options = "";
			if (!string.IsNullOrWhiteSpace(cfg.saveGamePath))
				options = "-l \"" + cfg.saveGamePath + "\"";
			else
			{
				if (string.IsNullOrWhiteSpace(cfg.configPath))
					throw new ArgumentException("Configuration is empty","cfg.configPath");
				options = "-c \"" + cfg.configPath + "\"";
			}
			options += " -p " + BasePort;
			if (cfg.isDependent)
				options += " -b";
			if (!cfg.startSession)
				options += " -o";

			RequiresHeartbeats = cfg.isDependent;

			server = Program.StartComponent("server", "server", options,true);

			if (cfg.isDependent)
				heartbeatSender = new HeartbeatSender(BasePort);

			LastUDPPing = DateTime.Now + TimeSpan.FromSeconds(10);
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

			if (RequiresHeartbeats)
			{
				if (heartbeatSender == null)
					heartbeatSender = new HeartbeatSender(BasePort);
			}
			else
				if (heartbeatSender != null)
				{
					heartbeatSender.Dispose();
					heartbeatSender = null;
				}

			if (!receiver.IsConnected && !SessionIsClosed)
				receiver.TryToReconnect();
			if (!controlConnection.IsAttemptingConnection && !controlConnection.IsConnected)
			{
				controlConnection.TryToReconnect();
			}

			base.SubItems[1].Text = ID.ToNiceString();
			var status = base.SubItems[4];
			var worldID = base.SubItems[2];
			var ai = base.SubItems[3];
			var rules = base.SubItems[5];
			base.Text = BasePort + ".." + (BasePort + NumPorts -1);
			status.Text = "";

			if (!aiIsEnabled)
				ai.Text = "No";
			else
			{
				string cp = aiScript;
				if (string.IsNullOrWhiteSpace(cp))
					ai.Text = "Yes";
				else
					ai.Text = "Lua:" + aiScript;
			}

			bool hasInfo = receiver.IsConnected && receiver.LastUpdate != null && DateTime.Now - receiver.LastUpdate.Received < TimeSpan.FromSeconds(2);

			if (!SessionIsClosed && hasInfo)
			{
				var info = receiver.LastUpdate.Package.game;
				status.Text += info.numFactionsOnline + "/" + info.numFactionsCreated + " online, Round "+info.roundNumber;
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
					status.Text += "Session closed or suspended";
				}
				else
				{
					worldID.Text = "?";
					rules.Text = "?";
					if (DateTime.Now - LastUDPPing > TimeSpan.FromSeconds(2))
						status.Text += "No contact";
					else
						status.Text += "Connecting...";
				}
			}

			if (RequiresHeartbeats)
				status.Text += " (dep)";



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
					if (heartbeatSender != null)
					{
						heartbeatSender.Dispose();
						heartbeatSender = null;
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

		#endregion


		internal void SetSession(bool active)
		{
			IssueControlCommand("/Server/Online/session = " + active);
			IssueControlCommand("/Server/Online/info = " + active);
		}

		public static bool PortIntersection(int a, int b)
		{
			return a <= b + NumPorts - 1 && a + NumPorts - 1 >= b;
		}

		public static bool PortContained(int basePort, int individualPort)
		{
			return basePort <= individualPort && basePort + NumPorts - 1 >= individualPort;
		}
	}
}
