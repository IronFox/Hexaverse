using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Logging;

namespace TechniteLogic
{
	internal class Client : Protocol.Client
	{

		private string authenticationString;
		/// <summary>
		/// Creates a new Client instance to communicate with a technite hosting server
		/// </summary>
		/// <param name="authenticationString">Secret to authenticate the local client</param>
        public Client(string authenticationString):base(new TcpClient(),false)
		{
			this.authenticationString = authenticationString;
		}


		protected override void OnReadThreadStart()
		{
			Interface.authenticate.SendTo(this, authenticationString);

		}

		protected override void OnClose()
		{
			Out.Log(Significance.ProgramFatal, "Connection lost to host. Exiting");
			Program.ShutDown(0);
		}

		public void Connect(ushort port)
		{
			try
			{ 
				TcpClient.Connect("localhost",port);
				StartNoSSLSelf();
			}
			catch (Exception ex)
			{
				Out.Log(Significance.ProgramFatal,"Unable to connect to service host on port "+port+": "+ex.ToString());
				Program.ShutDown(-1);
			}
		}

	}
}
