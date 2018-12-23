using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace ServerManager
{


	public class ServerInfoReceiver : LocalhostClient
	{
		public struct VersionPackage
		{
			public UInt64 a, b, c, d, e, f, g, h;
		}

		public struct HostPackage
		{
			public UUID hostID;
			public VersionPackage version;
			public UInt16 basePort;
		}


		public struct GameConfigPackage
		{
			public UInt32 gameFlags;
			public byte gameGoal;
			public byte worldDiscovery;
		}

		public struct GamePackage
		{
			public UInt16 numFactionsOnline;
			public UInt16 numFactionsCreated;
			public UInt32 roundNumber;
			public UUID worldID;
			public byte activePhase;
			public GameConfigPackage gameConfig;
		}

		public struct InfoPackage
		{
			public HostPackage host;
			public GamePackage game;
		}

		public class Update
		{
			public readonly InfoPackage Package;
			public readonly DateTime Received;

			public Update(InfoPackage p)
			{
				Package = p;
				Received = DateTime.Now;
			}
		}

		public Update LastUpdate { get; private set; } = null;


		public ServerInfoReceiver(int port) : base(port)
		{}


		private static Protocol.ChannelMap staticMap = new Protocol.ChannelMap();
		public override Protocol.ChannelMap ChannelMap => staticMap;

		public bool AiIsEnabled
		{
			get
			{
				var up = LastUpdate;
				return up != null && (up.Package.game.gameConfig.gameFlags & (uint)flag_t.NexusAI) != 0;
			}
		}

		static ServerInfoReceiver()
		{
			staticMap.Register<InfoPackage>(0, (client0, package) =>
			{
				var client = (ServerInfoReceiver)client0;
				client.LastUpdate = new Update(package);
			});
		}

		enum flag_t
		{
			ClosedGame = 0x1,
			BasesRequired = 0x2,
			StartBases = 0x4,
			NexusAI = 0x8,
			EMPZones = 0x10,
			SimulateLiquids = 0x20,
			TutorialMode = 0x40,
			GameHasEnded = 0x80,
			NexusHasBases = 0x100,
			NexusPortsOnAllPoles = 0x200,
			RequireServerPassword = 0x400,

			SupportSpectatorFactions = 0x1000,
			SupportRegularFactions = 0x2000,


		};

		public static string GameFlagsToString(uint gameFlags)
		{
			var values = (flag_t[])Enum.GetValues(typeof(flag_t));
			var names = Enum.GetNames(typeof(flag_t));
			List<String> result = new List<string>();
			for (int i = 0; i < values.Length; i++)
				if ((gameFlags & (uint)values[i]) != 0)
					result.Add(names[i]);
			return string.Join(", ", result);
		}


	}
}
