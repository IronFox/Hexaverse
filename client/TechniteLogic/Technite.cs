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

			public	Grid.HCellIdI	GetI()
			{
				return new Grid.HCellIdI(data >> 8, GetHeight());
			}
			public	Grid.HCellIdU	GetU()
			{
				return new Grid.HCellIdU(data >> 8, GetLayer());
			}

			public	int			GetHeight()
			{
				return Grid.CellStack.ToHeight(GetLayer());
			}

			public uint		GetLayer()
			{
				return data & 0xFF;
			}
		
			public uint	GetStackID()
			{
				return data >> 8;
			}

			public static implicit operator CompressedLocation(Grid.HCellIdI cellID)
			{
				return new CompressedLocation((cellID.StackID << 8) | Grid.CellStack.ToLayer(cellID.Height));
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


			public static implicit operator Resources(Protocol.Interface.TechniteResources res)
			{
				return new Resources(res.Energy,res.Matter);
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
					return new RelativeTarget(){neighborIndex = GetNeighborIndex(), heightDelta = GetHeightDelta() };
				}
			}

			public		CompressedTarget(byte data)
			{
				Data = data;
			}
			public		CompressedTarget(RelativeTarget target)
			{
				Data = (byte)((byte)target.neighborIndex | (byte)((target.heightDelta + 1) << 3));
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
			public uint neighborIndex;
			public int heightDelta;
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

		private void				ImportState(Protocol.Interface.TechniteState state)
		{
			nextTask = Task.None;
			taskParameter = 0;
			lastResources = resources;
			resources = state.Resources;
			taskResult = (TaskResult)state.TaskResult;
			this.state = new CompressedState(state.State).Decoded;
			exists = true;
		}



		private static Dictionary<Grid.HCellIdU,Technite>	map = new Dictionary<Grid.HCellIdU,Technite>();

		private static List<Technite>	all = new List<Technite>(), check = new List<Technite>();

		public static IEnumerable<Technite> All { get { return all; } }
		public static int Count { get {  return all.Count; } }

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

		public static void CreateOrUpdate(Protocol.Interface.TechniteState state)
		{
			Grid.HCellIdU loc = new CompressedLocation(state.Location).GetU();
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


		internal Protocol.Interface.TechniteInstruction ExportInstruction()
		{
			return new Protocol.Interface.TechniteInstruction()
			{
				NextTask = (byte)this.nextTask,
				TaskParameter = this.taskParameter,
				TaskTarget = new CompressedTarget(this.taskTarget).Data
			};
		}
	}
}
