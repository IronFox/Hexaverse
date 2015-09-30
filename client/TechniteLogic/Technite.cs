using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Logging;

namespace TechniteLogic
{
	class Technite
	{

		public static byte	EatEnergyCost = 1,
							SplitEnergyCost = 5,
							SplitMatterCost = 5,
							EnergyPackageSize = 5,
							MatterPackageSize = 5;


		public static byte[] MatterYield = new byte[Enum.GetValues(typeof(Grid.Content)).Length - 1];//discounting Undefined


		//static const BYTE	MatterYield[NumGroundTypes];
		//static const BYTE	ChangeTypeTo[NumGroundTypes];


		public static bool			HeldBy(Grid.Content content)
		{
			return content != Grid.Content.Clear && content != Grid.Content.Water && content != Grid.Content.Lava;
		}

		public struct CompressedLocation
		{
			private readonly UInt32		data;

			public UInt32	Data { get { return data; } }

			public CompressedLocation(UInt32 data)
			{
				this.data = data;
			}

			public	Grid.HCellID	GetU()
			{
				return new Grid.HCellID(data >> 8, (int)GetLayer());
			}


			public uint		GetLayer()
			{
				return data & 0xFF;
			}
		
			public uint	GetStackID()
			{
				return data >> 8;
			}


			public static implicit operator CompressedLocation(Grid.HCellID cellID)
			{
				return new CompressedLocation((cellID.StackID << 8) | (uint)cellID.Layer );
			}
		}

		public struct Resources
		{
			public byte		Energy,
							Matter;
			internal static readonly Resources Zero = new Resources(0,0);

			public 			Resources(int energy, int matter)
			{
				Energy = (byte)energy;
				Matter = (byte)matter;
			}


			public static implicit operator Resources(Interface.Struct.TechniteResources res)
			{
				return new Resources(res.energy,res.matter);
			}

			public static Resources	operator/(Resources res, int div)
			{
				return new Resources(res.Energy / div, res.Matter / div);

			}

			public static Resources		operator+(Resources a, Resources b)
			{
				return new Resources( Sum(a.Energy,b.Energy), Sum(a.Matter,b.Matter) );
			}

			private static byte Sum(byte a, byte b)
			{
				return (byte)Math.Min(0xFF, (int)a + (int)b);
			}

			public bool		Decrease(Resources by)
			{
				if (Matter < by.Matter || Energy < by.Energy)
					return false;
				Matter -= by.Matter;
				Energy -= by.Energy;
				return true;
			}

			public static bool operator>=(Resources a, Resources b)
			{
				return a.Energy >= b.Energy && a.Matter >= b.Matter;
			}
			public static bool operator <=(Resources a, Resources b)
			{
				return a.Energy <= b.Energy && a.Matter <= b.Matter;
			}

			public static bool operator==(Resources a, Resources b)
			{
				return a.Energy == b.Energy && a.Matter == b.Matter;
			}
			public static bool operator !=(Resources a, Resources b)
			{
				return a.Energy != b.Energy || a.Matter != b.Matter;
			}

			public override int GetHashCode()
			{
				int h = Energy.GetHashCode();
				h *= 257;
				h += Matter.GetHashCode();
				return h;
			}

			public override bool Equals(object obj)
			{
				return obj is Resources && ((Resources)obj == this);
			}

			public override string ToString()
			{
				return "Energy=" + Energy + ", Matter=" + Matter;
			}
		};

		internal static Technite Find(Grid.HCellID location)
		{
			Technite rs;
			if (map.TryGetValue(location, out rs))
				return rs;
			return null;
		}

		public static readonly Resources	SplitCost = new Resources(SplitEnergyCost,SplitMatterCost), 
												EatCost = new Resources(EatEnergyCost,0);


		public enum Task
		{
			None,
			EatSurroundingCell,
			GrowTo,
			TransferEnergyTo,
			TransferMatterTo,

			Count
		};

		public enum TaskResult
		{
			NothingToDo,
			BadCommand,
			CannotEatThis,
			CannotStandThere,
			TechniteLacksResources,
			NoTechniteAtDestination,
			MoreWorkNeeded,
			Again,
			Success
		};

		public struct CompressedTarget
		{
			public readonly byte Data;
			public static uint SelfNeighbor = 0xF;

			public RelativeTarget Decoded
			{ 
				get
				{ 
					return new RelativeTarget(GetNeighborIndex(), GetHeightDelta());
				}
			}

			public		CompressedTarget(byte data)
			{
				Data = data;
			}
			public		CompressedTarget(RelativeTarget target)
			{
				Data = (byte)((byte)target.NeighborIndex | (byte)((target.HeightDelta + 1) << 4));
			}

			public uint	GetNeighborIndex()
			{
				return (uint)(Data & 0xF);
			}
			public int			GetHeightDelta()
			{
				return (((int)(Data >> 4)) - 1);
			}

		}

		public struct CompressedState
		{
			public readonly byte Data;
		
			public		CompressedState(State st)
			{
				Data = (byte)((st.ttl & 0x7F) | (st.lit ? 0x80 : 0x0));
			}

			public		CompressedState(byte data)
			{
				Data = data;
			}

			public State Decoded
			{
				get
				{
					return new State(){lit = IsLit(), ttl = GetTTL()};
				}
			}

			public bool		IsLit()
			{
				return (Data & 0x80) != 0;
			}
			public byte		GetTTL()
			{
				return (byte)(Data & 0x7f);
			}

		}

		public struct State
		{
			public bool lit;
			public byte ttl;

			public override string ToString()
			{
				return "TTL=" + ttl + ", Lit=" + lit;
			}

		}


		/// <summary>
		/// Cell descriptor relative to a given cell. Some relative targets may be valid only from specific cells.
		/// </summary>
		public struct RelativeTarget
		{
			public readonly uint NeighborIndex;
			public readonly int HeightDelta;

			/// <summary>
			/// Neighbor index to be used when addressing own stack index. If used in combination with a height delta value of 0, the target describes the local cell itself
			/// </summary>
			public static uint SelfNeighbor = CompressedTarget.SelfNeighbor;

			public RelativeTarget(uint neighborIndex, int heightDelta)
			{
				if (neighborIndex > 0xF && neighborIndex != uint.MaxValue)
					throw new ArgumentOutOfRangeException("RelativeTarget expects the neighbor index to be passed as first parameter. Maximum degree is 15, but passed parameter is "+neighborIndex);
				if (neighborIndex == uint.MaxValue && heightDelta != 0)
					throw new ArgumentException("If neighbor index is uint.MaxValue, height delta must be set to 0");
				if (heightDelta > 1 || heightDelta < -1)
					throw new ArgumentOutOfRangeException("RelativeTarget expects a relative height delta to be passed as second parameter. This value must be in the range [-1,+1], but given value is " + heightDelta);
				NeighborIndex = neighborIndex;
				HeightDelta = heightDelta;
			}

			public override bool Equals(object obj)
			{
				return (obj is RelativeTarget) && ((RelativeTarget)obj) == this;
			}

			public static bool operator ==(RelativeTarget a, RelativeTarget b)
			{
				return a.HeightDelta == b.HeightDelta && a.NeighborIndex == b.NeighborIndex;
			}
			public static bool operator !=(RelativeTarget a, RelativeTarget b)
			{
				return a.HeightDelta != b.HeightDelta || a.NeighborIndex != b.NeighborIndex;
			}

			public override int GetHashCode()
			{
				int hash = 17;
				hash += NeighborIndex.GetHashCode();
				hash *= 31;
				hash += HeightDelta.GetHashCode();
				return hash;
			}

			public override string ToString()
			{
				if (NeighborIndex == SelfNeighbor)
				{
					return "<> |" + HeightDelta;
                }
				return "->" + NeighborIndex + " |" + HeightDelta;
			}

			public static Grid.HCellID operator+(Grid.HCellID cell, RelativeTarget t)
			{
				if (t.NeighborIndex == SelfNeighbor)
					return new Grid.HCellID(cell.StackID, cell.Layer + t.HeightDelta);
				return new Grid.HCellID(Grid.Graph.Nodes[cell.StackID].Neighbors[t.NeighborIndex], cell.Layer + t.HeightDelta);
			}

			public static readonly RelativeTarget Invalid = new RelativeTarget(uint.MaxValue, 0);
			public static readonly RelativeTarget Empty = new RelativeTarget(SelfNeighbor, 0);
        }



		Resources			resources, lastResources;
		TaskResult			taskResult = TaskResult.NothingToDo;
		
		public readonly Grid.HCellID	Location;
		public Resources CurrentResources { get { return resources; } }
		public Resources LastResources { get { return lastResources; } }
		public TaskResult LastTaskResult { get { return taskResult; } }


		Task				nextTask;
		byte				taskParameter;
		RelativeTarget		taskTarget;

		State				state;
		bool				exists = false;


		public State		Status {  get { return state;  } }


		public override string ToString()
		{
			return Location + " (" + state + ") {" + resources + "}";
		}



		public void SetNextTask(Task t, RelativeTarget target, byte parameter = 0)
		{
			Grid.HCellID absoluteTarget = Location + target;
			if (!absoluteTarget.IsValid)
			{
				Out.Log(Significance.Unusual, Location+": Trying to set invalid relative target "+target+". Ignoring command");
				return;
			}
			Grid.Content content = Grid.World.CellStacks[absoluteTarget.StackID].volumeCell[absoluteTarget.Layer].content;
			Out.Log(Significance.Low, "Setting next technite task to "+t+", "+target+", "+parameter+" (target cell is "+content+")");
			nextTask = t;
			taskParameter = parameter;
			taskTarget = target;
		}

		private /**/				Technite(Grid.HCellID loc)
		{
			Location = loc;
		}

		private void				ImportState(Interface.Struct.TechniteState state)
		{

			taskParameter = 0;
			lastResources = resources;
			resources = state.resources;
			taskResult = (TaskResult)state.taskResult;
			this.state = new CompressedState(state.state).Decoded;
			exists = true;
			if (taskResult != TaskResult.MoreWorkNeeded)
				nextTask = Task.None;

		}



		private static Dictionary<Grid.HCellID,Technite>	map = new Dictionary<Grid.HCellID,Technite>();

		private static List<Technite>	all = new List<Technite>(), check = new List<Technite>();

		public static IEnumerable<Technite> All { get { return all; } }
		public static int Count { get {  return all.Count; } }

		public bool CanSplit { get { return CurrentResources >= SplitCost;  } }
		public bool CanEat { get { return CurrentResources >= EatCost; } }

		public static byte MyFactionGridID { get; internal set; }
		public Task LastTask { get { return nextTask; } }

		public static void Reset()
		{
			check.Clear();
			foreach (var t in all)
			{ 
				t.exists = false;	//leave in dict for now
				check.Add(t);
			}
			all.Clear();
		}

		public static void CreateOrUpdate(Interface.Struct.TechniteState state)
		{
			Grid.HCellID loc = new CompressedLocation(state.location).GetU();
			Technite tech;
			if (map.TryGetValue(loc, out tech))
			{
				tech.ImportState(state);
				all.Add(tech);
				return;
			}
			tech = new Technite(loc);
			tech.ImportState(state);
			map.Add(loc,tech);
			all.Add(tech);
		}

		public static void Cleanup()
		{
			foreach (Technite t in check)
			{
				if (!t.exists)
				{
					map.Remove(t.Location);
				}
			}
			check.Clear();

			foreach (Technite t in all)
			{
				Grid.Content cellContent = Grid.World.CellStacks[t.Location.StackID].volumeCell[t.Location.Layer].content;
                if (cellContent != Grid.Content.Technite)
				{
					Out.Log(Significance.Unusual, "Expected technite content in cell "+t.Location+", but found "+cellContent);
				}
			}

			int badCount = 0;
			foreach (Grid.CellStack stack in Grid.World.CellStacks)
			{
				foreach (Grid.CellStack.Cell cell in stack.volumeCell)
				{
					if (cell.content == Grid.Content.Undefined)
					{
						badCount++;

					}

				}

			}
			if (badCount > 0)
				Out.Log(Significance.Unusual, "Found terrain holes in "+badCount+"/"+(Grid.World.CellStacks.Length*Grid.CellStack.LayersPerStack)+" locations");
		}


		internal Interface.Struct.TechniteInstruction ExportInstruction()
		{
			return new Interface.Struct.TechniteInstruction()
			{
				nextTask = (byte)this.nextTask,
				taskParameter = this.taskParameter,
				taskTarget = new CompressedTarget(this.taskTarget).Data
			};
		}
	}
}
