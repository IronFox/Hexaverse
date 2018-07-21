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
		private Process controlConsole, server;
		public UUID ID { get; private set; }
		public readonly int BasePort;
		public readonly ListViewItem ListViewItem = new ListViewItem();
		public DateTime LastUDPPing { get; private set; }

		public string Status
		{
			get
			{
				return ID.IsEmpty ? "Unknown" : "Ping " + LastUDPPing;
			}
		}

		public Server(int basePort)
		{
			BasePort = basePort;
			ListViewItem.Text = basePort + ".." + (basePort + 2);
				//basePort + "," + (basePort + 1) + "(," + (basePort + 2) + ")";
			ListViewItem.SubItems.Add("");	//server id
			ListViewItem.SubItems.Add("");  //world id
			ListViewItem.SubItems.Add("Unknown");  //status
			ListViewItem.SubItems.Add("");  //rules
		}

		public void Update(UDPReceiver.Info info)
		{
			ID = info.HostID;
			LastUDPPing = info.Received;
			ListViewItem.SubItems[1].Text = info.HostID.ToNiceString();
			ListViewItem.SubItems[3].Text = Status;
		}
	}
}
