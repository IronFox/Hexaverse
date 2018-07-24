using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ServerManager
{
	public partial class ServerTable : Form
	{
		public ServerTable()
		{
			InitializeComponent();
		}

		private void visibilityStripItem_Click(object sender, EventArgs e)
		{

		}


		private List<Action<string>> attributeListHandler = new List<Action<string>>();

		private void FlushAttributes()
		{
			var attribs = globalAttributeView.Items;
			attribs.Clear();
			attributeListHandler.Clear();
		}

		private void AddAttribute(string name, string value, string tooltip, Action<string> onClickHandleValue)
		{
			var row = globalAttributeView.Items.Add("");
			row.SubItems.Add(name + ":");
			row.SubItems.Add(value);
			row.ToolTipText = tooltip;
			if (onClickHandleValue != null)
				row.ToolTipText += ". Double click to open";
			attributeListHandler.Add(onClickHandleValue);

			globalAttributeView.ShowItemToolTips = true;

			attributeNameHeader.Width = -1;
			attributeValueHeader.Width = -1;
		}

		private UDPReceiver receiver;
		private string hsfDefaultDirectory;
		private void ServerTable_Load(object sender, EventArgs e)
		{
			FlushAttributes();
			AddAttribute("Version", Program.gameVersion, "Local Hexaverse version", null);
			AddAttribute("Install Dir", Directory.GetCurrentDirectory(), "Hexaverse Installation Directory", (dir) =>
			  {
				  Process.Start("explorer.exe", dir);
			  });
			{
				string myDocs = System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
				Program.gameDir = Path.Combine(myDocs, "My Games", "Hexaverse");
				hsfDefaultDirectory = hsfSaveFileDialog.InitialDirectory = Path.Combine(Program.gameDir, "shared", "saves");
				Directory.CreateDirectory(Program.gameDir);
				AddAttribute("Game Dir", Program.gameDir, "Hexaverse Game Directory", (dir) =>
				   {
					   Process.Start("explorer.exe", dir);
				   });
				string cfg = Path.Combine(Program.gameDir, "server", "config", "server.ini");
				AddAttribute("Server Cfg", cfg, "Server Configuration File", (path) =>
				   {
					   OpenServerConfiguration("Save Configuration", (path2) =>
					   {
						   File.Delete(path);
						   File.Move(path2, path);
					   });
				   });
			}

			receiver = new UDPReceiver();
			receiverTimer.Start();
		}


		private readonly List<Server> servers = new List<Server>();
		private readonly Dictionary<int, Server> portMap = new Dictionary<int, Server>();

		private void OpenServerConfiguration(string commitText, Action<string> onCompleteHandlePath)
		{
			new EditServerConfig().Show(commitText, onCompleteHandlePath);
		}

		private void globalAttributeView_DoubleClick(object sender, EventArgs e)
		{
			if (globalAttributeView.SelectedIndices.Count == 1)
			{
				var at = globalAttributeView.SelectedIndices[0];
				if (at < attributeListHandler.Count && attributeListHandler[at] != null)
					attributeListHandler[at](globalAttributeView.Items[at].SubItems[2].Text);
			}
		}


		private delegate void StringDelegate(string str);

		private void LogStatus(string msg)
		{
			if (mainStatusStrip.InvokeRequired)
			{
				mainStatusStrip.Invoke(new StringDelegate(LogStatus), new object[] { msg });
				return;
			}
			toolStripStatusLabel.Text = msg;
		}

		private void ShowError(string msg)
		{
			if (InvokeRequired)
			{
				Invoke(new StringDelegate(ShowError), new object[] { msg });
				return;
			}
			ShowError("Error", msg);
		}

		private void ShowError(string caption, string msg)
		{
			MessageBox.Show(msg, caption, MessageBoxButtons.OK, MessageBoxIcon.Error);
		}

		private void receiverTimer_Tick(object sender, EventArgs e)
		{
			UDPReceiver.Info info;
			while (receiver.Updates.TryDequeue(out info))
			{
				Server server;
				if (!portMap.TryGetValue(info.Port, out server))
				{
					server = new Server(info.Port, LogStatus, ShowError);
					servers.Add(server);
					portMap.Add(info.Port, server);
					serverListView.Items.Add(server);
				}
				server.Receive(info);
			}
			for (int i = 0; i < servers.Count; i++)
			{
				var server = servers[i];
				if (!server.Update())
				{
					serverListView.Items.Remove(server);
					servers.RemoveAt(i--);
					portMap.Remove(server.BasePort);
					server.Dispose();
				}
			}
			ruleHeader.Width = -1;
		}

		void Shutdown()
		{
			foreach (var server in servers)
			{
				server.Dispose();
			}
			servers.Clear();
			portMap.Clear();
			if (receiver != null)
				receiver.Dispose();
			receiver = null;
			Application.Exit();
		}

		private void exitToolStripMenuItem_Click(object sender, EventArgs e)
		{
			Close();
		}

		private void ServerTable_FormClosed(object sender, FormClosedEventArgs e)
		{
			Shutdown();
		}

		private void serverContextMenuStrip_Opening(object sender, CancelEventArgs e)
		{
			var items = serverListView.SelectedItems;
			if (items.Count != 1)
			{
				controlStripItem.Enabled = false;
				logStripItem.Enabled = false;
				saveGameAsToolStripMenuItem.Enabled = false;
				if (items.Count == 0)
				{
					shutdownStripItem.Enabled = false;
					closeSessionStripItem.Enabled = false;
					resumeSessionStripItem.Enabled = false;
				}
				else
				{
					shutdownStripItem.Enabled = true;
					closeSessionStripItem.Enabled = true;
					resumeSessionStripItem.Enabled = true;
				}
			}
			else
			{
				controlStripItem.Enabled = true;
				logStripItem.Enabled = true;
				saveGameAsToolStripMenuItem.Enabled = true;
				shutdownStripItem.Enabled = true;
				var closed = ((Server)serverListView.SelectedItems[0]).SessionIsClosed;
				closeSessionStripItem.Enabled = !closed;
				resumeSessionStripItem.Enabled = closed;
			}
		}

		private void shutdownStripItem_Click(object sender, EventArgs e)
		{
			foreach (Server srv in serverListView.SelectedItems)
			{
				srv.Shutdown();
			}
		}

		private void controlStripItem_Click(object sender, EventArgs e)
		{
			foreach (Server srv in serverListView.SelectedItems)
			{
				srv.StartControlConsole();
			}
		}

		private void logStripItem_Click(object sender, EventArgs e)
		{
			foreach (Server srv in serverListView.SelectedItems)
			{
				srv.OpenLogFile();
			}
		}

		private void saveGameAsToolStripMenuItem_Click(object sender, EventArgs e)
		{
			var rs = hsfSaveFileDialog.ShowDialog();
			if (rs == DialogResult.OK || rs == DialogResult.Yes)
			{
				((Server)serverListView.SelectedItems[0]).IssueControlCommand("/IO/saveTo \"" + hsfSaveFileDialog.FileName + "\"");
			}
		}

		private void launchClientButton_Click(object sender, EventArgs e)
		{
			Program.RunComponent("client", "Visual Client");
		}

		private void closeSessionStripItem_Click(object sender, EventArgs e)
		{
			foreach (Server srv in serverListView.SelectedItems)
			{
				srv.SetSession(false);
			}
		}

		private void resumeSessionStripItem_Click(object sender, EventArgs e)
		{
			foreach (Server srv in serverListView.SelectedItems)
			{
				srv.SetSession(true);
			}
		}

		private void launchServerButton_Click(object sender, EventArgs e)
		{
			new StartServer().Show(hsfDefaultDirectory,(port) =>
			{
				foreach (var s in servers)
				{
					if (Server.PortIntersection(s.BasePort, port))
						return false;
				}
				return true;
			},
			(Server.Config cfg) =>
			{
				var server = new Server(cfg, LogStatus, ShowError);
				servers.Add(server);
				portMap.Add(server.BasePort, server);
				serverListView.Items.Add(server);
			}
			);
			
		}

		private void ServerTable_FormClosing(object sender, FormClosingEventArgs e)
		{
			bool any = false;
			foreach (var server in servers)
			{
				if (server.RequiresHeartbeats)
				{
					any = true;
					break;
				}
			}
			if (any)
			{
				var rs = MessageBox.Show("One or more servers are dependent on this manager. If you close it down, these servers may also shut down. Are you sure you wish to close the manager?", "Please confirm", MessageBoxButtons.OKCancel,MessageBoxIcon.Warning);
				if (rs != DialogResult.OK)
					e.Cancel = true;
			}
		}
	}
}
