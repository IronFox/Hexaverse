using System;
using Consensus.Universe;
using Renci.SshNet;
using System.IO;
using System.Collections.Generic;

namespace Consensus.IO
{
	public class Server
	{
		internal static Server Allocate(OrbitingBody orbitingBody)
		{
			throw new NotImplementedException();
		}

		SshClient sshClient;
		PrivateKeyFile key;

		public readonly string Hostname;

		public Server(string hostname)
		{
			string path = Path.Combine("keys", hostname);
			if (!File.Exists(path))
				throw new FileNotFoundException("Key file not found for " + hostname, path);
			key = new PrivateKeyFile(path);
			sshClient = new SshClient(hostname, "consensus", key);
			Hostname = hostname;
		}

		internal void Unlock(OrbitingBody orbitingBody)
		{
			RunControlCommand("/Consensus/unlockWorld " + orbitingBody.ID, null);// "Locklevel decreased for " + orbitingBody.ID);
		}

		public void RunControlCommand(string cmd, string lookForResult)
		{
			lock (this)
			{
				sshClient.Connect();
				try
				{
					SshCommand scmd = sshClient.RunCommand("WINEDEBUG=-all wine ~/ControlConsole/ControlConsole.exe -c " + cmd);
					string rawResult = scmd.Result;
					string[] rs = rawResult.Split(new char[] { '\n', '\r' });
					string errors = null;
					foreach (var line in rs)
					{
						if (line.Length == 0)
							continue;
						if (line[0] == '-')
						{
							if (errors == null)
								errors = line.Substring(1);
							else
								errors += '\n' + line.Substring(1);
						}
						else
							if (line[0] == '+')
						{
							if (line.Substring(1) == lookForResult)
								return;
						}

					}
					if (errors != null)
						throw new Exception(errors);
					if (lookForResult != null)
						throw new Exception("Expected result ('" + lookForResult + "') not found in '" + rawResult + "'");
				}
				catch
				{
					sshClient.Disconnect();
					throw;
				}
				sshClient.Disconnect();
			}
		}

		internal LockedServerLink AcquireLockTo(OrbitingBody orbitingBody)
		{
			lock(this)
			{
				RunControlCommand("/Consensus/lockWorld " + orbitingBody.ID, "Locklevel increased for "+orbitingBody.ID);

				return new LockedServerLink(this,orbitingBody);
			}

		}
	}
}