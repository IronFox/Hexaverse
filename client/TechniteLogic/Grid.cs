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
			Clear,


			Undefined //client-only. used for debugging
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

				public Vec3 GetLocation(int layerIndex)
				{
					return StackBase + StackDirection * (CellStack.HeightPerLayer * layerIndex);
				}
			}

			public static Node[] Nodes { get; private set; }

			public static Vec3 GetLocation(HCellID loc)
			{
				return Nodes[loc.StackID].GetLocation(loc.Layer);
			}

			private static List<Node> nodeBuffer;


			public static void AddChunk(Interface.Struct.NodeChunk chunk)
			{
				//Out.Log(Significance.Common, "Received node-chunk, containing "+chunk.nodes.Length+" nodes");
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

		/// <summary>
		/// Checks whether or not a block is solid.
		/// If true, then it can definitely support a technite, even if it stands free otherwise
		/// </summary>
		/// <param name="content"></param>
		/// <returns></returns>
		internal static bool IsSolid(Content content)
		{
			return content == Content.Earth 
				|| content == Content.Foundation 
				|| content == Content.Granite 
				|| content == Content.Grass 
				|| content == Content.Road 
				|| content == Content.Rock
				|| content == Content.Sand
				|| content == Content.Snow 
				;
		}
		/// <summary>
		/// Checks whether a technite can (theoretically) stand on this block.
		/// Currently Content.Unknown and Content.Technite are classified as theoretical.
		/// Technites should spawn atop theoretically solid blocks only, if they absolutely have to,
		/// or other geometry can hold them
		/// </summary>
		/// <param name="content"></param>
		/// <returns></returns>
		internal static bool IsTheoreticallySolid(Content content)
		{
			return IsSolid(content)
				|| content == Content.Unknown
				|| content == Content.Technite
				;
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
				Debug.Assert(!isSetUp);
				isSetUp = true;
				HeightPerLayer = heightPerLayer;
				LayersPerStack = numLayersPerStack;
				Out.Log(Significance.Common, "CellStack.Setup: height per layer="+heightPerLayer+", layers per stack="+numLayersPerStack);
            }

			internal bool IsShaded(uint layer)
			{
				for (uint l = layer + 1; l < LayersPerStack; l++)
					if (volumeCell[l].content != Content.Clear)
						return true;
				return false;
			}

			public Cell[] volumeCell = new Cell[LayersPerStack];


			public CellStack()
			{
				for (int i = 0; i < volumeCell.Length; i++)
					volumeCell[i] = new Cell() { content = Content.Undefined, structureCount = 0, techniteFactionID = 0 };
			}

            public struct Cell
            {
				public Content content;
				public byte structureCount;
				public byte techniteFactionID;


				internal void ApplyContentDelta(byte value)
				{
					if (value != 0 || content == Content.Undefined)
						content = (Content)(value);
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

		public static class World
		{

			public static void Create()
			{
				if (CellStacks == null)
				{
					CellStacks = new CellStack[Graph.Nodes.Length];
					for (int i = 0; i < CellStacks.Length; i++)
						CellStacks[i] = new CellStack();
				}
			}

			public static CellStack[] CellStacks
			{
				get; private set;
			}

			public static Content CoreContent { get; private set; }


			private static void ApplyDeltaField(Interface.Struct.GridDelta delta, Interface.Struct.GridDeltaBlock[] blocks, Action<CellStack, uint, byte> action)
			{
				if (blocks.Length != 0)
				{
					//Out.Log(Significance.Low, "  Applying " + blocks.Length + " blocks...");
					uint at = 0;
					foreach (var block in blocks)
					{
						for (int i = 0; i < block.repitition; i++)
						{
							uint stackIndex = (at % delta.nodeCount) + delta.nodeOffset;
							uint layer = at / delta.nodeCount;

							CellStack stack = CellStacks[stackIndex];
							action(stack, layer, block.value);

							at++;
						}
					}
					Debug.Assert(at == delta.nodeCount * CellStack.LayersPerStack);
				}
			}


			public static void ApplyDelta(Interface.Struct.GridDelta delta)
			{
				if (CellStacks == null)
				{
					Out.Log(Significance.ProgramFatal, "World not initialized. Cannot apply delta.");
					return;
				}
				if (delta.nodeOffset + delta.nodeCount > Graph.Nodes.Length)
				{
					Out.Log(Significance.ProgramFatal, "Invalid node range for world delta: ["+delta.nodeOffset+","+(delta.nodeOffset+delta.nodeCount)+") /"+ Graph.Nodes.Length);
					return;
				}
				Out.Log(Significance.Common, "Applying world delta at range "+delta.nodeOffset+"..."+(delta.nodeOffset+delta.nodeCount)+ " /"+Graph.Nodes.Length);


				ApplyDeltaField(delta, delta.contentBlocks, (stack, layer, value) => stack.volumeCell[layer].ApplyContentDelta(value));
				ApplyDeltaField(delta, delta.structureCountBlocks, (stack, layer, value) => stack.volumeCell[layer].ApplyStructureDelta(value));
				ApplyDeltaField(delta, delta.techniteFactionBlocks, (stack, layer, value) => stack.volumeCell[layer].ApplyTechniteDelta(value));

			}

			internal static void Setup(Content coreContent)
			{
				CoreContent = coreContent;
				Out.Log(Significance.Common, "Set world core content to "+CoreContent);
			}
		}

		

		public static void ApplyDelta(Interface.Struct.GridDelta delta)
		{
			World.Create();
			World.ApplyDelta(delta);
		}


		/// <summary>
		/// Volume cell identifier using (unsigned) integers to describe the layer (0..CellStack.LayersPerStack-1) this cell resides on
		/// </summary>
		public struct HCellID
		{
			public readonly uint StackID;

			/// <summary>
			/// Residual layer of this cell in the range [0,CellStack.LayersPerStack)
			/// Theoretically, a uint would be more appropriate. Practically applying relative targets causes too many weird cases with uint.
			/// </summary>
			public readonly int Layer;

			public static readonly HCellID Invalid = new HCellID(uint.MaxValue,int.MaxValue);

			public HCellID(uint stackID, int layer)
			{
				StackID = stackID;
				Layer = layer;
			}


			public bool IsValid {  get { return StackID < Graph.Nodes.Length && Layer >= 0 && Layer < CellStack.LayersPerStack; } }

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
				if (!(obj is HCellID))
					return false;
				return ((HCellID)obj) == this;
			}

			public static bool operator==(HCellID a, HCellID b)
			{
				return a.StackID == b.StackID && a.Layer == b.Layer;
			}

			public static bool operator !=(HCellID a, HCellID b)
			{
				return a.StackID != b.StackID || a.Layer != b.Layer;
			}

			public override string ToString()
			{
				return StackID + "[" + Layer + "]";
			}
		}

		internal static void BeginSession(float heightPerLayer, int numLayersPerStack)
		{
			CellStack.Setup(heightPerLayer, numLayersPerStack);
		}
	}
}
