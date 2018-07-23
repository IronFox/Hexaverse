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
	public partial class StartServer : Form
	{
		public StartServer()
		{
			InitializeComponent();
		}


		const int Disallowed0 = 10245;
		const int Disallowed1 = 10246;
		const int Default = 10247;

		private Func<int, bool> basePortIsFreeFunction;

		private string configFileName;
		private Action<int, bool, string, string> startServer;

		public void Show(Func<int, bool> basePortIsFreeFunction, Action<int,bool, string, string> startServer)
		{
			configFileName = Path.GetTempFileName();

			this.basePortIsFreeFunction = basePortIsFreeFunction;
			this.startServer = startServer;

			this.portCheckTimer.Start();

			this.Show();
		}

		private bool portOkay = false;

		private void RecheckBasePort()
		{
			try
			{
				int port = int.Parse(this.portInput.Text);
				if (port <= 1024 || port+Server.NumPorts >= 65536)
				{
					FailPort("Port out of valid range [1024,65535]");
					return;
				}
				if (!basePortIsFreeFunction(port) || Server.PortContained(port, Disallowed0) || Server.PortContained(port, Disallowed1))
				{
					FailPort("Port range already in use");
					return;
				}
				portInput.BackColor = SystemColors.Window;
				portStatusLabel.Text = "Port OK";
				portOkay = true;
				this.startButton.Enabled = configOkay;
			}
			catch (FormatException)
			{
				FailPort("Unable to parse input");
			}
		}

		private void FailPort(string error)
		{
			portOkay = false;
			portInput.BackColor = Color.Red;
			portStatusLabel.Text = error;
			startButton.Enabled = false;
		}

		private void startButton_Click(object sender, EventArgs e)
		{
			RecheckBasePort();
			if (!portOkay)
			{
				return;
			}
			Debug.Assert(File.Exists(configFileName));

			startServer(int.Parse(portInput.Text), dependentCheck.Checked, configFileName, "");
			configFileName = null;
			Close();
		}

		private void portCheckTimer_Tick(object sender, EventArgs e)
		{
			RecheckBasePort();
		}

		private void portInput_TextChanged(object sender, EventArgs e)
		{
			RecheckBasePort();
		}


		private EditServerConfig editor = null;
		private bool configOkay = false;

		private void configureButton_Click(object sender, EventArgs e)
		{
			if (editor == null)
				editor = new EditServerConfig();

			if (editor.Visible)
				editor.BringToFront();
			else
				editor.Show(configFileName, "Accept Config", (str) => { configOkay = true; startButton.Enabled = portOkay; }); //don't care
		}

		private void StartServer_FormClosing(object sender, FormClosingEventArgs e)
		{
			if (editor != null)
			{
				editor.Close();
				editor.Dispose();
				editor = null;
				configOkay = false;
				if (configFileName != null)
				{
					File.Delete(configFileName);
					configFileName = null;
				}
			}
		}
	}
}
