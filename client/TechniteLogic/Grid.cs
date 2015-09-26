using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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
				public uint[] Neighbors { get; private set; }

			}



			public static Node[]	Nodes { get; private set;}



		}


		public class CellStack
		{
			public const uint Range = 32;	//range up and down. effective range is thus [-Range/2,+Range/2]
			public const int HalfRange = (int)(Range >> 1);
			public const int TopHeight = HalfRange - 1,
								BottomHeight = -HalfRange;



			public static int ToHeight(uint layer)
			{
				return (int)(layer) - HalfRange;
			}

			public static uint ToLayer(int height)
			{
				return (uint)(height < -HalfRange ? 0 : height + HalfRange);
			}




		}


		/// <summary>
		/// High-resolution volume cell identifier using signed integers to describe the height level (-HalfRange..HalfRange-1) this cell resides on
		/// </summary>
		public struct HCellIdI
		{
			public readonly uint	StackID;
			public readonly int		Height;

			public static readonly HCellIdI Invalid = new HCellIdI(uint.MaxValue, int.MaxValue);

			public HCellIdI(uint stackID, int height)
			{
				StackID = stackID;
				Height = height;
			}

			private HCellIdI(uint stackID, uint layer)
			{
				StackID = 0xffffffff;
				Height = 0;
			}

			public static bool operator ==(HCellIdI a, HCellIdI b)
			{
				return a.StackID == b.StackID && a.Height == b.Height;
			}

			public static bool operator !=(HCellIdI a, HCellIdI b)
			{
				return a.StackID != b.StackID || a.Height != b.Height;
			}

			public override int GetHashCode()
			{
				int rs = 19;
				rs += StackID.GetHashCode();
				rs *= 37;
				rs += Height.GetHashCode();
				return rs;
			}

			public override bool Equals(object obj)
			{
				if (!(obj is HCellIdI))
					return false;
				return ((HCellIdI)obj) == this;
			}

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

	}
}
