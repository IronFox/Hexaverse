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

		private readonly Action<string> log, displayError;

		public ControlConnection(int port,Action<string> asyncLog, Action<string> asyncDisplayError) : base(port)
		{
			log = asyncLog;
			displayError = asyncDisplayError;
		}


		private void OnError(string error)
		{ }

		private void OnResponse(string response)
		{
			//World snapshot, round 0, written to [Documents]\My Games\Hexaverse\shared\saves\test.hsf in 0.17119 seconds

			//logFile: 'E:\Documents\My Games\Hexaverse\server\logs\server 2018 07 22 124802.v0.9.2.420.log'
			var logFileMatch = Regex.Match(response, @"^([^':\r\n]+): '?([^'\r\n]*)'?$");
			if (logFileMatch.Success && logFileMatch.Groups.Count == 3)
			{
				Action<string> onValue;
				if (onValueMap.TryRemove(logFileMatch.Groups[1].Value, out onValue))
				{
					var v = logFileMatch.Groups[2].Value;
					if (v.StartsWith("'") && v.EndsWith("'"))
						v = v.Substring(1, v.Length - 2);
					onValue(v);
				}
				return;
			}
			{
				var saveGameMatch = Regex.Match(response, @"^World snapshot, round ([0-9]+), written to (.*) in ([\.0-9]+) seconds$");
				if (saveGameMatch.Success && saveGameMatch.Groups.Count == 4)
				{
					log("Save game of round "+ saveGameMatch.Groups[1].Value+" written to "+ saveGameMatch.Groups[2].Value);
					return;
				}
			}
			{
				//"Unable to write world to "+FileSystem::PathToString(name)
				var matches = Regex.Match(response, @"^Unable to write world to (.*)$");
				if (matches.Success && matches.Groups.Count == 2)
				{
					displayError("Unable to write save game to " + matches.Groups[1].Value);
					return;
				}
				if (response == "Unable to save game right now. Not in suitable phase")
				{
					displayError(response);
					return;
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


		public void SetValue<T>(string path, T value)
		{
			ExecuteCommand("set '"+path.Replace("'", "\\'")+"' '" + value.ToString().Replace("'", "\\'") + "'");
		}

		public void GetValue(string path, Action<string> asyncOnValue)
		{
			onValueMap.TryAdd(path, asyncOnValue);
			cmdChannel.SendTo(this, path);
		}


	}
}
