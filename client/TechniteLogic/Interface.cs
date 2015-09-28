using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Math3D;
using Protocol;

namespace TechniteLogic
{
	public static class Interface
	{

		public static Protocol.OutChannel<Struct.TechniteInstructionChunk> techniteInstructionChunk = new Protocol.OutChannel<Struct.TechniteInstructionChunk>(Protocol.ChannelID.TechniteInstructionChunk);

	    public static void Register()
        {
			ChannelMap.Register<Struct.TechniteStateChunk>(ChannelID.TechniteStateChunk, Event.TechniteStateChunk);
			ChannelMap.RegisterSignal(ChannelID.InstructTechnites,Event.InstructTechnites);
			ChannelMap.Register<Struct.SessionBegin>(ChannelID.SessionBegin, Event.SessionBegin);
			ChannelMap.Register<Struct.NodeChunk>(ChannelID.NodeChunk, Event.NodeChunk);
		}


		public class Struct
		{

			public struct TechniteResources
			{
				public byte energy,
											matter;


			}

			public struct TechniteState
			{
				public UInt32 location;
				public TechniteResources resources;
				public byte taskResult,
											state;

			}

			public struct TechniteStateChunk
			{
				public bool isFirst;
				public TechniteState[] states;

			}

			public struct TechniteInstruction
			{
				public byte nextTask,
							taskTarget,
							taskParameter;
			}

			public struct TechniteInstructionChunk
			{
				public const int MaxPerChunk = 10000;
				public UInt32 offset;
				public TechniteInstruction[] instructions;
			}


			public struct GridDeltaBlock
			{
				public byte value;
				public UInt32 repitition;
			}

			public struct GridDelta
			{
				public int nodeOffset,
							nodeCount;
				public GridDeltaBlock[] contentBlocks,
										structureCountBlocks,
										techniteFactionBlocks;

			}

			public struct GridNode
            {
                public uint[] neighbors;
                public Vec3 stackBase;
                public Vec3 stackDirection;
            }

			public struct GridConfig
			{
				public float heightPerLayer;
				public int numLayersPerStack;
			}

			public struct NodeChunk
			{
				public bool isLast;
				public GridNode[] nodes;
			}

			public struct SessionBegin
			{
				public GridConfig grid;
				public byte myFactionGridID;
			}
        }

        public static class Event
		{

			public static void InstructTechnites(Protocol.Client cl)
			{
				Technite.Cleanup();	//updates must be done by now
				Logic.ProcessTechnites();

				int numTechnites = Technite.Count;
				int numChunks = numTechnites / Struct.TechniteInstructionChunk.MaxPerChunk;
				if ((numTechnites % Struct.TechniteInstructionChunk.MaxPerChunk)!= 0)
					numChunks++;

				var e = Technite.All.GetEnumerator();
				int offset = 0;
				for (int i = 0; i < numChunks; i++)
				{
					int chunkSize = Math.Min(Struct.TechniteInstructionChunk.MaxPerChunk, numTechnites - offset);

					Struct.TechniteInstructionChunk chunk = new Struct.TechniteInstructionChunk();
					chunk.offset = (uint)offset;
					chunk.instructions = new Struct.TechniteInstruction[chunkSize];
					for (int j = 0; j < chunkSize; j++)
					{
						Technite t = e.Current;
						chunk.instructions[j] = t.ExportInstruction();
						bool success = e.MoveNext();
						Debug.Assert(success || (i+1 == numChunks && j+1== chunkSize));
					}

					techniteInstructionChunk.SendTo(cl,chunk);

					offset += chunkSize;
				}
			}

			public static void TechniteStateChunk(Protocol.Client cl, Struct.TechniteStateChunk chunk)
			{
				if (chunk.isFirst)
					Technite.Reset();
				foreach (var state in chunk.states)
					Technite.CreateOrUpdate(state);

			}

			internal static void NodeChunk(Protocol.Client cl, Struct.NodeChunk nodeChunk)
			{
				Grid.Graph.AddChunk(nodeChunk);
			}

			internal static void SessionBegin(Protocol.Client cl, Struct.SessionBegin sessionBegin)
			{
				Grid.BeginSession(sessionBegin.grid.heightPerLayer, sessionBegin.grid.numLayersPerStack);
				Technite.MyFactionGridID = sessionBegin.myFactionGridID;
			}
		}
	}
}
