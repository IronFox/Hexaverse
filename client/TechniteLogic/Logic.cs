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

		private static Technite.RelativeTarget EvaluateChoices(Grid.HCellID location, Func<Technite.RelativeTarget,Grid.HCellID, Grid.CellStack,int> f)
		{
	//		Out.Log(Significance.Low, "Observing neighborhood of " + location);
            Technite.RelativeTarget rs = Technite.RelativeTarget.Invalid;
            int quality = -1;
			uint[] choices = Grid.Graph.Nodes[location.StackID].Neighbors;
			for (uint i = 0; i < choices.Length; i++)
			{
				uint choice = choices[i];
				Grid.CellStack stack = Grid.World.CellStacks[choice];
				for (int h = -1; h <= 1; h++)
				{
					int layer = (location.Layer + h);
					if (layer < 0 || layer >= Grid.CellStack.LayersPerStack)
						continue;
					Grid.HCellID cellLocation = new Grid.HCellID(choice, layer);
					Technite.RelativeTarget target = new Technite.RelativeTarget(i, h);
//					Out.Log(Significance.Low, "Observing cell "+cellLocation+" ("+target+") with content="+stack.volumeCell[layer].content);

					int q = f(target, cellLocation, stack);
					if (q > quality)
					{
						quality = q;
						rs = target;
					}
				}
			}
			{
				uint choice = location.StackID;
				Grid.CellStack stack = Grid.World.CellStacks[choice];
				for (int h = -1; h <= 1; h++)
				{
					if (h == 0)
						continue;
					int layer = (location.Layer + h);
					if (layer < 0 || layer >= Grid.CellStack.LayersPerStack)
						continue;
					Grid.HCellID cellLocation = new Grid.HCellID(choice, layer);
					Technite.RelativeTarget target = new Technite.RelativeTarget(Technite.RelativeTarget.SelfNeighbor, h);
//					Out.Log(Significance.Low, "Observing cell " + cellLocation + " (" + target + ") with content=" + stack.volumeCell[layer].content);

					int q = f(target, cellLocation, stack);
					if (q > quality)
					{
						quality = q;
						rs = target;
					}
				}
			}
			return rs;
		}

		private static Technite.RelativeTarget GetLitOrUpperTechnite(Grid.HCellID location)
		{
			return EvaluateChoices(location, (relative, cell, stack) =>
			{
				Grid.Content content = stack.volumeCell[cell.Layer].content;
				if (content != Grid.Content.Technite)
					return -1;
				Technite other = Technite.Find(cell);
				if (other == null)
				{
					Out.Log(Significance.Unusual, "Located neighboring technite in "+cell+", but cannot find reference to class instance");
					return -1;
				}
				int rs = 0;
				if (other.Status.lit)
					rs++;
				rs += relative.HeightDelta;
				return rs;
			}
			);
		}

		private static Technite.RelativeTarget GetUnlitOrLowerTechnite(Grid.HCellID location)
		{
			return EvaluateChoices(location, (relative, cell, stack) =>
			{
				Grid.Content content = stack.volumeCell[cell.Layer].content;
				if (content != Grid.Content.Technite)
					return -1;
				Technite other = Technite.Find(cell);
				if (other == null)
				{
					Out.Log(Significance.Unusual, "Located neighboring technite in " + cell + ", but cannot find reference to class instance");
					return -1;
				}
				int rs = 1;
				if (other.Status.lit)
					rs--;
				rs -= relative.HeightDelta;
				return rs;
			}
			);
		}

		private static Technite.RelativeTarget GetBestFoodChoice(Grid.HCellID location)
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
		private static Technite.RelativeTarget GetSplitTarget(Grid.HCellID location)
		{
			return EvaluateChoices(location, (relative, cell, stack) =>
			{
				Grid.Content content = stack.volumeCell[cell.Layer].content;
				int rs = 5;
				if (content != Grid.Content.Clear && content != Grid.Content.Water)
					rs -= 2;
				if (cell.Layer + 1 < Grid.CellStack.LayersPerStack && stack.volumeCell[cell.Layer + 1].content == Grid.Content.Technite)
					return -1;	//probably a bad idea to split here
				Grid.Content ground = cell.Layer <= 0 ? Grid.World.CoreContent : stack.volumeCell[cell.Layer - 1].content;
                if (Grid.IsTheoreticallySolid(ground))
				{
					int bonus = 0;
					if (Grid.IsSolid(ground))
						bonus += 3;
					return relative.HeightDelta + 1 + bonus + rs;
				}
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
				Out.Log(Significance.Low, "Processing "+t);


				if (t.LastTaskResult == Technite.TaskResult.MoreWorkNeeded)
				{
					bool skip = false;
					switch (t.LastTask)
					{
						case Technite.Task.ConsumeSurroundingCell:
							skip = t.CanConsume;
							break;
						case Technite.Task.GrowTo:
							skip = t.CanSplit;
							break;
					}
					if (skip)
					{
						Out.Log(Significance.Common, "Still busy doing last job ("+t.LastTask+"). Allowing technite to continue");
						continue;
					}
				}
				bool tryTransfer = false;
				if (t.CanSplit)
				{
					Technite.RelativeTarget target = GetSplitTarget(t.Location);
					if (target != Technite.RelativeTarget.Invalid)
					{
						t.SetNextTask(Technite.Task.GrowTo, target);
					}
					else
					{
						Out.Log(Significance.Unusual, "Unable to find adequate splitting destination");
						tryTransfer = true;
					}
				}
				else
					if (t.CanGnawAt)
					{
						Technite.RelativeTarget target = GetBestFoodChoice(t.Location);
						if (target != Technite.RelativeTarget.Invalid)
						{
							t.SetNextTask(Technite.Task.GnawAtSurroundingCell, target);
						}
						else
						{
							Out.Log(Significance.Unusual, "Unable to find adequate eating destination");
							tryTransfer = true;
						}
					}
				else
				{
					Out.Log(Significance.Unusual, "Insufficient resources to do anything");
					tryTransfer = t.CurrentResources != Technite.Resources.Zero;
				}

				if (tryTransfer)
				{
					Technite.RelativeTarget target = Technite.RelativeTarget.Invalid;
					Technite.Task task;
                    if (t.CurrentResources.Matter > t.CurrentResources.Energy)
					{
						Out.Log(Significance.Low, "Trying to transfer matter");
						task = Technite.Task.TransferMatterTo;
						target = GetLitOrUpperTechnite(t.Location);
					}
					else
					{
						Out.Log(Significance.Low, "Trying to transfer energy");
						task = Technite.Task.TransferEnergyTo;
						target = GetUnlitOrLowerTechnite(t.Location);

					}
					if (target != Technite.RelativeTarget.Invalid)
					{
						t.SetNextTask(task, target);
					}
					else
					{
						Out.Log(Significance.Unusual, "Unable to find adequate transfer target");
						tryTransfer = true;
					}


				}


			}


		}
	}
}
