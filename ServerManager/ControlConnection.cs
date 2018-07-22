using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace ServerManager
{
	class ControlConnection : LocalhostClient
	{
		public enum Channel
		{
			Command,    //c2s : string
			Complete,   //both : string
			Error,      //s2c : string
			Context,    //s2c : string
			Response,   //s2c : string
			ExecutionDone,  //s2c : <signal>
		};


		private static Protocol.ChannelMap staticMap = new Protocol.ChannelMap();
		public override Protocol.ChannelMap ChannelMap => staticMap;
		private static Protocol.OutChannel<string> cmdChannel = new Protocol.OutChannel<string>((uint)Channel.Command);

		static ControlConnection()
		{
			staticMap.Register<string>((uint)Channel.Complete, (client0, package) =>
			{
				var client = (ControlConnection)client0;
			});
			staticMap.Register<string>((uint)Channel.Error, (client0, package) =>
			{
				var client = (ControlConnection)client0;
				client.OnError(package);
			});
			staticMap.Register<string>((uint)Channel.Context, (client0, package) =>
			{
				var client = (ControlConnection)client0;
			});
			staticMap.Register<string>((uint)Channel.Response, (client0, package) =>
			{
				var client = (ControlConnection)client0;
				client.OnResponse(package);
			});
			staticMap.RegisterSignal((uint)Channel.ExecutionDone, (client0) =>
			{
				var client = (ControlConnection)client0;
				client.OnExecutionDone();

			});
		}

		private readonly ConcurrentDictionary<string, Action<string>> onValueMap = new ConcurrentDictionary<string, Action<string>>();


		public ControlConnection(int port) : base(port)
		{}


		private void OnError(string error)
		{ }

		private void OnResponse(string response)
		{
			//logFile: 'E:\Documents\My Games\Hexaverse\server\logs\server 2018 07 22 124802.v0.9.2.420.log'
			var match = Regex.Match(response, @"^([^':\r\n]+): '([^'\r\n]+)'");
			if (match.Success && match.Groups.Count == 3)
			{
				bool brk = true;

				Action<string> onValue;
				if (onValueMap.TryRemove(match.Groups[1].Value, out onValue))
				{
					onValue(match.Groups[2].Value);
				}
			}
		}

		protected override void OnReadThreadStart()
		{
			foreach (var pair in onValueMap)
				cmdChannel.SendTo(this, pair.Key);
		}

		private void OnExecutionDone()
		{

		}

		public void ExecuteCommand(string cmd)
		{
			cmdChannel.SendTo(this, cmd);
			
		}

		public void GetValue(string path, Action<string> asyncOnValue)
		{
			onValueMap.TryAdd(path, asyncOnValue);
			cmdChannel.SendTo(this, path);
		}


	}
}
