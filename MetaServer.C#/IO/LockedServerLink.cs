using System;

namespace Consensus.IO
{
	public class LockedServerLink : IDisposable
	{
		private Server server;
		bool locked = false;
		readonly Universe.OrbitingBody body;

		public LockedServerLink(Server server, Universe.OrbitingBody body)
		{
			this.server = server;
			locked = true;
			this.body = body;
		}

		~LockedServerLink()
		{
			Dispose();
		}

		public void Command(string cmd, string lookForResult)
		{
			if (!locked)
				throw new Exception("Cannot execute command. Lock has been disengaged.");
			server.RunControlCommand(cmd, lookForResult);
		}

		public void Release()
		{
			Dispose();
		}

		public void Dispose()
		{
			if (!locked)
				return;
			server.Unlock(body);
			locked = false;
			GC.SuppressFinalize(this);
		}
	}
}