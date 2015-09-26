using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Logging;
using System.Diagnostics;

namespace TechniteLogic
{
	class Program
	{
		public static volatile bool ShuttingDown = false;


		public static void ShutDown(int returnCode)
		{
			ShuttingDown = true;
			Logging.Out.EndService();
			//Console.ReadKey();
			Environment.Exit(returnCode);
		}


		static void onCancel(object sender, ConsoleCancelEventArgs e)
		{
			ShutDown(0);
		}



		static void Main(string[] args)
		{
			Console.CancelKeyPress += new ConsoleCancelEventHandler(onCancel);
			//Logging.Out.StartService();	//if multiple instances are started, the binding to a fixed ip is bad, so no service


			if (args.Length < 2)
			{
				Console.Error.WriteLine("Missing port parameter. Exiting");
				ShutDown(-1);
				return;
			}

			ushort serverPort;

			if (!ushort.TryParse(args[1], out serverPort))
			{
				Console.Error.WriteLine("Unable to parse parameter '"+args[1]+"' to port number. Exiting");
				ShutDown(-1);
				return;
			}

			Client client = new Client();
			System.Threading.Thread.Sleep(100);
			Out.Log(Logging.Significance.Important, "Connecting to server on port "+serverPort);
			client.Connect(serverPort);

			ShutDown(0);
		}




	}
}
