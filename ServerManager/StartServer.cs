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
		private string saveGamePath;
		private Action<Server.Config> startServer;

		public void Show(string saveGamePath, Func<int, bool> basePortIsFreeFunction, Action<Server.Config> startServer)
		{
			configFileName = Path.GetTempFileName();
			EditServerConfig.GenerateConfigAt(configFileName);
			configOkay = false;
			saveGameOkay = false;
			portOkay = false;
			startButton.Enabled = false;
			//loadSaveButton.Text = "Load Save Game";

			this.saveGamePath = saveGamePath;
			this.basePortIsFreeFunction = basePortIsFreeFunction;
			this.startServer = startServer;

			RecheckBasePort();
			if (!portOkay)
				fixPortButton_Click(null, null);

			this.portCheckTimer.Start();

			this.Show();
		}

		private bool portOkay = false;

		private bool PortIsInUse(int port)
		{
			return !basePortIsFreeFunction(port) || Server.PortContained(port, Disallowed0) || Server.PortContained(port, Disallowed1);
		}

		private void RecheckBasePort()
		{
			try
			{
				int port = int.Parse(this.portInput.Text);
				if (port < 1024 || port+Server.NumPorts > 49151 - Server.NumPorts)
				{
					FailPort("Port out of valid range [1024,"+(49151-Server.NumPorts)+"]");
					return;
				}
				if (PortIsInUse(port))
				{
					FailPort("Port range already in use");
					return;
				}
				portInput.BackColor = SystemColors.Window;
				portStatusLabel.Text = "Port OK";
				portOkay = true;
				fixPortButton.Visible = false;
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
			fixPortButton.Visible = true;
		}

		private void startButton_Click(object sender, EventArgs e)
		{
			RecheckStartButton();
			if (!startButton.Enabled)
			{
				return;
			}
			Debug.Assert(File.Exists(configFileName));

			Server.Config cfg = new Server.Config();
			cfg.configPath = newGameButton.Enabled ? configFileName : null;
			cfg.eraseConfigOnceLoaded = true;
			cfg.saveGamePath = loadSaveButton.Enabled ? loadHSFDialog.FileName : null;
			cfg.isDependent = dependentCheck.Checked;
			cfg.port = int.Parse(portInput.Text);
			cfg.startSession = !startOfflineCheck.Checked;

			startServer(cfg);
			configFileName = null;
			Close();
		}

		private void portCheckTimer_Tick(object sender, EventArgs e)
		{
			RecheckStartButton();
		}

		private void portInput_TextChanged(object sender, EventArgs e)
		{
			RecheckStartButton();
		}


		private EditServerConfig editor = null;
		private bool configOkay = false;
		private bool saveGameOkay = false;

		private void configureButton_Click(object sender, EventArgs e)
		{

			if (editor != null && editor.Visible)
				editor.BringToFront();
			else
			{
				editor = new EditServerConfig();
				editor.Show(configFileName, "Accept Config", (str) => { configOkay = true; RecheckStartButton(); });
			}
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

		private bool CanStart()
		{
			RecheckBasePort();
			if (!portOkay)
				return false;
			if (loadSaveButton.Checked)
				return saveGameOkay;
			return configOkay;
		}

		private void RecheckStartButton()
		{
			startButton.Enabled = CanStart();
		}

		private void loadSaveButton_CheckedChanged(object sender, EventArgs e)
		{
			pickSaveGameButton.Enabled = loadSaveButton.Checked;
			RecheckStartButton();
		}

		private void newGameButton_CheckedChanged(object sender, EventArgs e)
		{
			configureButton.Enabled = newGameButton.Checked;
			RecheckStartButton();
		}

		private void pickSaveGameButton_Click(object sender, EventArgs e)
		{
			if (string.IsNullOrWhiteSpace(loadHSFDialog.InitialDirectory))
				loadHSFDialog.InitialDirectory = saveGamePath;

			var rs = loadHSFDialog.ShowDialog();
			if (rs == DialogResult.OK || rs == DialogResult.Yes)
			{
				saveGameOkay = File.Exists(loadHSFDialog.FileName);
				RecheckStartButton();
				if (saveGameOkay)
				{
					var segs = loadHSFDialog.FileName.Split(Path.DirectorySeparatorChar);
					//pickSaveGameButton.Text = segs.Last();
					saveGameLabel.Text = segs.Last();
					//if (loadSaveButton.Text.Length > 32)
						//loadSaveButton.Text = loadSaveButton.Text.Substring(0, 32);
				}
			}
		}

		private void abortButton_Click(object sender, EventArgs e)
		{
			Close();
		}

		private void fixPortButton_Click(object sender, EventArgs e)
		{
			int current;
			try
			{
				current = int.Parse(portInput.Text);
			}
			catch
			{
				current = Default;
			}
			if (current < 1024)
				current = 1024;
			if (current+Server.NumPorts > 49151)
				current = Default;
			while (PortIsInUse(current))
				current++;
			portInput.Text = current.ToString();
		}
	}
}
