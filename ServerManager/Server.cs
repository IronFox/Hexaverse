using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ServerManager
{
	public class Server
	{
		private Process controlConsole=null, server=null;
		private readonly ServerInfoReceiver receiver;
		public UUID ID { get; private set; }
		public bool SessionIsClosed { get; private set; }
		public bool RequiresHeartbeats { get; private set; }
		public readonly int BasePort;
		public readonly ListViewItem ListViewItem = new ListViewItem();
		public DateTime LastUDPPing { get; private set; }



		public Server(int basePort)
		{
			BasePort = basePort;
			ListViewItem.Text = basePort + ".." + (basePort + 2);
				//basePort + "," + (basePort + 1) + "(," + (basePort + 2) + ")";
			ListViewItem.SubItems.Add("");	//server id
			ListViewItem.SubItems.Add("");  //world id
			ListViewItem.SubItems.Add("Unknown");  //status
			ListViewItem.SubItems.Add("");  //rules

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
			if (!receiver.IsConnected && !SessionIsClosed)
				receiver.TryToReconnect();

			ListViewItem.SubItems[1].Text = ID.ToNiceString();
			var status = ListViewItem.SubItems[3];
			var worldID = ListViewItem.SubItems[2];
			var rules = ListViewItem.SubItems[4];
			ListViewItem.Text = BasePort + ".." + (BasePort + 2);
			if (RequiresHeartbeats)
				ListViewItem.Text += " (H)";

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
	}
}
