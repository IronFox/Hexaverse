using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Logging;

namespace TechniteLogic
{
	public class Logic
	{

		private static Technite.RelativeTarget EvaluateChoices(Grid.HCellIdU location, Func<Technite.RelativeTarget,Grid.HCellIdU, Grid.CellStack,int> f)
		{
			Technite.RelativeTarget rs = Technite.RelativeTarget.Invalid;
            int quality = -1;
			uint[] choices = Grid.Graph.Nodes[location.StackID].Neighbors;
			for (uint i = 0; i < choices.Length; i++)
			{
				uint choice = choices[i];
				Grid.CellStack stack = Grid.World.cellStack[choice];
				for (int h = -1; h <= 1; h++)
				{
					if (location.Layer == 0 && h == -1)
						continue;
					uint layer = (uint)(location.Layer + h);
					if (layer >= Grid.CellStack.LayersPerStack)
						continue;
					Technite.RelativeTarget target = new Technite.RelativeTarget(i, h);
					int q = f(target, new Grid.HCellIdU(choice, layer), stack);
					if (q > quality)
					{
						quality = q;
						rs = target;
					}
				}
			}
			return rs;
		}

		private static Technite.RelativeTarget GetBestFoodChoice(Grid.HCellIdU location)
		{
			return EvaluateChoices(location, (relative, cell, stack) =>
			{
				Grid.Content content = stack.volumeCell[cell.Layer].content;
				byte yield = Technite.MatterYield[(int)content];
				if (cell.Layer + 1 >= Grid.CellStack.LayersPerStack)
					return yield;
				if (stack.volumeCell[cell.Layer + 1].content != Grid.Content.Technite)
					return yield;
				return -1;
			}
			);
		}
		private static Technite.RelativeTarget GetClearNeighbor(Grid.HCellIdU location)
		{
			return EvaluateChoices(location, (relative, cell, stack) =>
			{
				Grid.Content content = stack.volumeCell[cell.Layer].content;
				if (content != Grid.Content.Clear)
					return -1;
				if (cell.Layer == 0 || Grid.IsSolid(stack.volumeCell[cell.Layer - 1].content))
					return relative.HeightDelta + 1 ;
				return -1;
			}
			);
		}

		public static void ProcessTechnites()
		{
			Out.Log(Significance.Common, "ProcessTechnites()");

			//let's do some simple processing

			foreach (Technite t in Technite.All)
			{
				if (t.LastTaskResult == Technite.TaskResult.MoreWorkNeeded)
					continue;
				//try digging horizontally
				if (t.CanSplit)
				{
					Technite.RelativeTarget target = GetClearNeighbor(t.Location);
					if (target != Technite.RelativeTarget.Invalid)
					{
						t.SetNextTask(Technite.Task.GrowTo, target);
					}
				}
				else
					if (t.CanEat)
					{
						Technite.RelativeTarget target = GetBestFoodChoice(t.Location);
						if (target != Technite.RelativeTarget.Invalid)
						{
							t.SetNextTask(Technite.Task.EatSurroundingCell, target);
						}
					}
				else
				{
					//Technite.Resources res = t.CurrentResources;
					//if (!res.Empty)
					//{
					//	if (res.Energy > res.Matter)
					//	{


					//	}

					//}
				}


			}


		}
	}
}
