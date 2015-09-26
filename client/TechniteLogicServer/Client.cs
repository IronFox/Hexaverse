using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace TechniteLogic
{
	internal class Client : Protocol.Client
	{

		private static List<Client>	all = new List<Client>();
		private static List<Client> dump = new List<Client>();
		//private static Mutex allLock = new Mutex();

		private Client(TcpClient client):base(client,false)
		{}


		public static void AddNew(TcpClient client)
		{
			Client cl = new Client(client);
			cl.StartNoSSL();
			Monitor.Enter(all);
				all.Add(cl);
			Monitor.Exit(all);
		}

		protected virtual override void OnClose()
		{
			if (Monitor.TryEnter(all))
			{
				all.Remove(this);
				Monitor.Exit(all);
			}
			else
				lock(dump)
					dump.Add(this);
		}


		public static void VisitAll(Action<Client> visitor)
		{
			Monitor.Enter(all);
				foreach (var cl in all)
					visitor(cl);
				lock(dump)
				{
					foreach (Client cl in dump)
						all.Remove(cl);
					dump.Clear();
				}
			Monitor.Exit(all);
		}
	}
}
