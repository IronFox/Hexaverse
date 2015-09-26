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
		static TcpListener listener;
		static Thread listenThread;

		public static volatile bool ShuttingDown = false;


		public static void ShutDown()
		{
			ShuttingDown = true;
			if (listener != null)
				listener.Stop();
		}


		public static void ReportUnexpected(Client origin, Exception ex, Logging.Significance significance = Logging.Significance.ClientFatal)
		{
			//Debug.Fail(ex.ToString());
			Log(origin, significance, ex.ToString());
			//more later
		}
		public static void ReportCommon(Client origin, Exception ex)
		{
			//Debug.Fail(ex.ToString());
			Log(origin, Logging.Significance.Low, ex.ToString());
			//more later
		}

		public static void Log(Client origin, Logging.Significance significance, String message)
		{
			Logging.Out.Log(origin != null ? origin.ToString() : "", origin != null && origin.Account != null ? origin.Account.GUID.ToString() : "", origin != null && origin.Account != null ? origin.Account.Name : "", significance, message);

		}


		public static void ReportFatal(Client origin, Exception ex)
		{
			ReportUnexpected(origin, ex, Logging.Significance.ProgramFatal);
			ShutDown();
		}

		public static void LogServiceDenial(Client requester, String errorMessage, object additionalParameter = null)
		{
			errorMessage = "Client request denied: '" + errorMessage + "'";
			if (additionalParameter != null)
				errorMessage += " (with parameter='" + additionalParameter + "')";
			Log(requester, Logging.Significance.Unusual, errorMessage);
		}
		static void Listen()
		{
			// Is someone trying to call us? Well answer!
			Log(null, Logging.Significance.Low, "Starting listening thread on " + listener.LocalEndpoint.ToString());
			while (!ShuttingDown)
			{
				try
				{
					Client.AddNew(listener.AcceptTcpClient());
				}
				catch (InvalidOperationException ex)
				{
					if (!ShuttingDown)
						ReportFatal(null, ex);
				}
				catch (SocketException ex)
				{
					if (!ShuttingDown)
						ReportFatal(null, ex);
				}
				catch (Exception ex)
				{
					ReportUnexpected(null, ex, Logging.Significance.Unusual);
				}
			}
			Log(null, Logging.Significance.Important, "Closing down listening thread");
		}

		private static volatile bool didEnd = false;

		static void end()
		{
			listenThread.Join();
			lock (listenThread)
			{
				if (!didEnd)
				{
					didEnd = true;
					Debug.Assert(ShuttingDown); //this should really be implied
					ShuttingDown = true;

					Client.VisitAll(client =>
					{
						client.ForceDisconnect();
					});
				}
			}
			Logging.Out.Close();
		}

		static void onCancel(object sender, ConsoleCancelEventArgs e)
		{
			ShutDown();
			end();
		}



		static void Main(string[] args)
		{
			Console.CancelKeyPress += new ConsoleCancelEventHandler(onCancel);

			Interface.In.Register();


			listener = new TcpListener(System.Net.IPAddress.Any, 0); //IPv4
			listener.Start();

			listenThread = new Thread(new ThreadStart(Listen));
			listenThread.Start();

			//            TimeServer.Start(TimeServerPort);
			//            ViewServer.Start(ViewServerPort);

			Logging.Out.Start();

			System.Threading.Thread.Sleep(100);

			//World.Save();

			Log(null, Logging.Significance.Important, "Initialization phase completed. Awaiting clients.");


			end();
		}



		static void Main(string[] args)
		{
		}
	}
}
