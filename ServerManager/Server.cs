using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ServerManager
{
	public class Server : ListViewItem, IDisposable
	{
		private Process controlConsole=null, server=null;
		private readonly ServerInfoReceiver receiver;
		public UUID ID { get; private set; }
		public bool SessionIsClosed { get; private set; }
		public bool RequiresHeartbeats { get; private set; }
		public readonly int BasePort;
		public DateTime LastUDPPing { get; private set; }
		private bool isDisposed = false;


		public Server(int basePort)
		{
			BasePort = basePort;
			base.Text = basePort + ".." + (basePort + 2);
			//basePort + "," + (basePort + 1) + "(," + (basePort + 2) + ")";
			base.SubItems.Add("");  //server id
			base.SubItems.Add("");  //world id
			base.SubItems.Add("Unknown");  //status
			base.SubItems.Add("");  //rules

			receiver = new ServerInfoReceiver(basePort);
			//receiver.TryToReconnect();
		}

		public void Receive(UDPReceiver.Info info)
		{
			ID = info.HostID;
			LastUDPPing = info.Received;
			SessionIsClosed = info.SessionClosed;
			RequiresHeartbeats = info.RequiresHeartbeats;
		}

		public bool Update()
		{
			if (this.isDisposed)
				return false;
			if (!receiver.IsConnected && !SessionIsClosed)
				receiver.TryToReconnect();

			base.SubItems[1].Text = ID.ToNiceString();
			var status = base.SubItems[3];
			var worldID = base.SubItems[2];
			var rules = base.SubItems[4];
			base.Text = BasePort + ".." + (BasePort + 2);
			if (RequiresHeartbeats)
				base.Text += " (H)";

			if (!SessionIsClosed && receiver.IsConnected && receiver.LastUpdate != null && DateTime.Now - receiver.LastUpdate.Received < TimeSpan.FromSeconds(2))
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

		public void Dispose()
		{
			isDisposed = true;
			if (receiver != null)
				receiver.Dispose();
			//if (server != null)	//it'll stop on its own
				//server.Kill();

		}

		public void IssueControlCommand(string cmd)
		{

		}

		public void FetchVariableContent(string path, Action<string> result)
		{

		}

		public void Shutdown()
		{
			IssueControlCommand("/server/shutdown");
		}

		internal void StartControlConsole()
		{
			try
			{
				if (Environment.Is64BitOperatingSystem)
					Process.Start(Path.Combine("ControlConsole", "ControlConsole.exe"), "-p " + BasePort);
				else
					Process.Start(Path.Combine("ControlConsole", "ControlConsole x86.exe"), "-p " + BasePort);
			}
			catch (Exception ex)
			{
				MessageBox.Show(ex.Message, "Error opening control console", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
			}
		}

		internal void OpenLogFile()
		{
			FetchVariableContent("/IO/logFile",(logAt) =>
			{
				Process.Start(logAt);
			});
		}
	}
}
