using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Math3D;
using System.Diagnostics;
using Logging;

namespace TechniteLogic
{
	public static class Grid
	{

		public enum Content
		{
			Unknown,
			Granite,
			Earth,
			Grass,
			Rock,
			Sand,
			Snow,
			Foundation,
			Road,
			Lava,
			Technite,
			Water,
			Clear
		}


		public static class Graph
		{
			public struct Node
			{
                public readonly uint[] Neighbors;
                public readonly Vec3 StackBase;
                public readonly Vec3 StackDirection;

                public Node(uint[] neighbors, Vec3 stackBase, Vec3 stackDirection)
                {
                    Neighbors = neighbors;
                    StackBase = stackBase;
                    StackDirection = stackDirection;
                }

				public static implicit operator Node(Interface.Struct.GridNode node)
				{
					return new Node(node.neighbors, node.stackBase, node.stackDirection);
				}

                public Vec3 GetLocation(uint layerIndex)
                {
                    return StackBase + StackDirection * (CellStack.HeightPerLayer * layerIndex);
                }
			}

			public static Node[]	Nodes { get; private set;}

            public static Vec3 GetLocation(HCellIdU loc)
            {
                return Nodes[loc.StackID].GetLocation(loc.Layer);
            }

			private static List<Node> nodeBuffer;


			public static void AddChunk(Interface.Struct.NodeChunk chunk)
			{
				Debug.Assert(World == null);
				Out.Log(Significance.Common, "Received node-chunk, containing "+chunk.nodes.Length+" nodes");
				if (nodeBuffer == null)
					nodeBuffer = new List<Node>();
                for (int i = 0; i < chunk.nodes.Length; i++)
					nodeBuffer.Add(chunk.nodes[i]);
				if (chunk.isLast)
				{
					Out.Log(Significance.Common, "Received last node-chunk. Completing graph");
					Nodes = nodeBuffer.ToArray();
					nodeBuffer = null;
				}
			}

			//public static void Setup(Interface.Struct.GridNode[] nodes)
			//{
			//	Debug.Assert(Nodes == null);
			//	Nodes = new Node[nodes.Length];
			//	for (int i = 0; i < nodes.Length; i++)
			//		Nodes[i] = nodes[i];
			//}
		}


		public class CellStack
		{
			public static float HeightPerLayer
			{
				get; private set;
			}

			public static int LayersPerStack
			{
				get; private set;
			}

			private static bool isSetUp = false;
			public static void Setup(float heightPerLayer, int numLayersPerStack)
			{
				Debug.Assert(World==null);
				Debug.Assert(!isSetUp);
				isSetUp = true;
				HeightPerLayer = heightPerLayer;
				LayersPerStack = numLayersPerStack;
				Out.Log(Significance.Common, "CellStack.Setup: height per layer="+heightPerLayer+", layers per stack="+numLayersPerStack);
            }


			public Cell[] volumeCell = new Cell[LayersPerStack];


			public CellStack()
			{
				for (int i = 0; i < volumeCell.Length; i++)
					volumeCell[i] = new Cell() { content = Content.Clear, structureCount = 0, techniteFactionID = 0 };
			}

            public struct Cell
            {
				public Content content;
				public byte structureCount;
				public byte techniteFactionID;

				internal void ApplyContentDelta(byte value)
				{
					if (value != 0)
						content = (Content)(value - 1);
				}

				internal void ApplyStructureDelta(byte value)
				{
					if (value != 0)
						structureCount = (byte)(value - 1);
				}

				internal void ApplyTechniteDelta(byte value)
				{
					if (value != 0)
						techniteFactionID = (byte)(value - 1);
				}
			}
		}

		public class WorldData
		{
			public CellStack[] cellStack = new CellStack[Graph.Nodes.Length];



			private void ApplyDeltaField(Interface.Struct.GridDelta delta, Interface.Struct.GridDeltaBlock[] blocks, Action<CellStack, uint, byte> action)
			{
				if (delta.contentBlocks.Length != 0)
				{
					uint at = 0;
					foreach (var block in blocks)
					{
						for (int i = 0; i < block.repitition; i++)
						{
							uint stackIndex = (at % delta.nodeCount) + delta.nodeOffset;
							uint layer = at / delta.nodeCount;

							CellStack stack = cellStack[stackIndex];
							action(stack, layer, block.value);

							at++;
						}
					}
					Debug.Assert(at == delta.nodeCount * CellStack.LayersPerStack);
				}
			}


			public void ApplyDelta(Interface.Struct.GridDelta delta)
			{
				Debug.Assert(delta.nodeOffset + delta.nodeCount <= Graph.Nodes.Length);
				Out.Log(Significance.Common, "Applying world delta at range "+delta.nodeOffset+"..."+(delta.nodeOffset+delta.nodeCount));

				ApplyDeltaField(delta, delta.contentBlocks, (stack, layer, value) => stack.volumeCell[layer].ApplyContentDelta(value));
				ApplyDeltaField(delta, delta.structureCountBlocks, (stack, layer, value) => stack.volumeCell[layer].ApplyStructureDelta(value));
				ApplyDeltaField(delta, delta.techniteFactionBlocks, (stack, layer, value) => stack.volumeCell[layer].ApplyTechniteDelta(value));
			}

		}


		/// <summary>
		/// Global world state. Must be set non-null by the time technite logic is evaluated
		/// </summary>
		public static WorldData World { get; private set; }


		public static void ApplyDelta(Interface.Struct.GridDelta delta)
		{
			if (World == null)
			{
				Out.Log(Significance.Common,"Creating world");
				World = new WorldData();
			}
			World.ApplyDelta(delta);
		}


		/// <summary>
		/// High-resolution volume cell identifier using unsigned integers to describe the layer (0..Range-1) this cell resides on
		/// </summary>
		public struct HCellIdU
		{
			public readonly uint StackID;
			public readonly uint Layer;

			public static readonly HCellIdU Invalid = new HCellIdU(uint.MaxValue,uint.MaxValue);

			public HCellIdU(uint stackID, uint layer)
			{
				StackID = stackID;
				Layer = layer;
			}

			private HCellIdU(uint stackID, int height)
			{
				StackID = 0xffffffff;
				Layer = 0;
			}

			public override int GetHashCode()
			{
				int rs = 19;
				rs += StackID.GetHashCode();
				rs *= 37;
				rs += Layer.GetHashCode();
				return rs;
			}

			public override bool Equals(object obj)
			{
				if (!(obj is HCellIdU))
					return false;
				return ((HCellIdU)obj) == this;
			}

			public static bool operator==(HCellIdU a, HCellIdU b)
			{
				return a.StackID == b.StackID && a.Layer == b.Layer;
			}

			public static bool operator !=(HCellIdU a, HCellIdU b)
			{
				return a.StackID != b.StackID || a.Layer != b.Layer;
			}
		}

		internal static void BeginSession(float heightPerLayer, int numLayersPerStack)
		{
			CellStack.Setup(heightPerLayer, numLayersPerStack);
		}
	}
}
