using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Security.Cryptography;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Net;
using System.Net.Sockets;
using System.IO;
using System.Diagnostics;


namespace Probe
{
	public partial class Form1 : Form
	{
		public Form1()
		{
			InitializeComponent();
		}


		Client	client;

		public static bool Callback(Object sender, X509Certificate cert, X509Chain chain, SslPolicyErrors sslPolicyErrors)
		{
			MessageBox.Show(sslPolicyErrors.ToString());
			return true;
		}



		private void connect_Click(object sender, EventArgs e)
		{
			Network.Setup(this);
			if (client != null)
				client.ForceDisconnect();

			client = Client.Connect(host.Text,null,userName.Text);
		}

		delegate void LogCallback(string text);


		public void Log(string line)
		{
			if (logOut.InvokeRequired)
			{
				LogCallback d = new LogCallback(Log);
				this.Invoke(d, new object[] { line });
			}
			else
			{
				logOut.Text += "\r\n" + line;
			}
		}
	}
}
