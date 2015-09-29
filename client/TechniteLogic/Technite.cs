using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace TechniteLogic
{
	class Technite
	{

		public static byte	EatEnergyCost = 1,
							SplitEnergyCost = 5,
							SplitMatterCost = 5,
							EnergyPackageSize = 5,
							MatterPackageSize = 5;


		public static byte[] MatterYield = new byte[Enum.GetValues(typeof(Grid.Content)).Length];


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

			public	Grid.HCellIdU	GetU()
			{
				return new Grid.HCellIdU(data >> 8, GetLayer());
			}


			public uint		GetLayer()
			{
				return data & 0xFF;
			}
		
			public uint	GetStackID()
			{
				return data >> 8;
			}


			public static implicit operator CompressedLocation(Grid.HCellIdU cellID)
			{
				return new CompressedLocation((cellID.StackID << 8) | cellID.Layer );
			}
		}

		public struct Resources
		{
			public byte		Energy,
							Matter;


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
		};


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
				Data = (byte)((byte)target.NeighborIndex | (byte)((target.HeightDelta + 1) << 3));
			}

			public uint	GetNeighborIndex()
			{
				return (uint)(Data & 0x7);
			}
			public int			GetHeightDelta()
			{
				return (((int)(Data >> 3)) - 1);
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
		}

		public struct RelativeTarget
		{
			public readonly uint NeighborIndex;
			public readonly int HeightDelta;

			public RelativeTarget(uint neighborIndex, int heightDelta)
			{
				if (neighborIndex >= 0xFF && neighborIndex != uint.MaxValue)
					throw new ArgumentOutOfRangeException("RelativeTarget expects the neighbor index to be passed as first parameter. Maximum degree is 255, but passed parameter is "+neighborIndex);
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

			public static readonly RelativeTarget Invalid = new RelativeTarget(uint.MaxValue, 0);
        }



		Resources			resources, lastResources;
		TaskResult			taskResult = TaskResult.NothingToDo;
		
		public readonly Grid.HCellIdU	Location;
		public Resources CurrentResources { get { return resources; } }
		public Resources LastResources { get { return lastResources; } }
		public TaskResult LastTaskResult { get { return taskResult; } }


		Task				nextTask;
		byte				taskParameter;
		RelativeTarget		taskTarget;

		State				state;
		bool				exists = false;






		public void SetNextTask(Task t, RelativeTarget target, byte parameter = 0)
		{
			nextTask = t;
			taskParameter = parameter;
			taskTarget = target;
		}

		private /**/				Technite(Grid.HCellIdU loc)
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



		private static Dictionary<Grid.HCellIdU,Technite>	map = new Dictionary<Grid.HCellIdU,Technite>();

		private static List<Technite>	all = new List<Technite>(), check = new List<Technite>();

		public static IEnumerable<Technite> All { get { return all; } }
		public static int Count { get {  return all.Count; } }

		public bool CanSplit { get { return CurrentResources >= SplitCost;  } }
		public bool CanEat { get { return CurrentResources >= EatCost; } }

		public static byte MyFactionGridID { get; internal set; }

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
			Grid.HCellIdU loc = new CompressedLocation(state.location).GetU();
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
