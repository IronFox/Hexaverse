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
			Out.Log(Significance.Low, "Observing neighborhood of " + location);
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
					Out.Log(Significance.Low, "Observing cell "+cellLocation+" ("+target+") with content="+stack.volumeCell[layer].content);

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
		private static Technite.RelativeTarget GetClearNeighbor(Grid.HCellID location)
		{
			return EvaluateChoices(location, (relative, cell, stack) =>
			{
				Grid.Content content = stack.volumeCell[cell.Layer].content;
				if (content != Grid.Content.Clear && content != Grid.Content.Water)
					return -1;
				Grid.Content ground = cell.Layer <= 0 ? Grid.World.CoreContent : stack.volumeCell[cell.Layer - 1].content;
                if (Grid.IsTheoreticallySolid(ground))
				{
					int bonus = 0;
					if (Grid.IsSolid(ground))
						bonus += 3;
					return relative.HeightDelta + 1;
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
						case Technite.Task.EatSurroundingCell:
							skip = t.CanEat;
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
				//try digging horizontally
				if (t.CanSplit)
				{
					Technite.RelativeTarget target = GetClearNeighbor(t.Location);
					if (target != Technite.RelativeTarget.Invalid)
					{
						t.SetNextTask(Technite.Task.GrowTo, target);
					}
					else
						Out.Log(Significance.Unusual, "Unable to find adequate splitting destination");
				}
				else
					if (t.CanEat)
					{
						Technite.RelativeTarget target = GetBestFoodChoice(t.Location);
						if (target != Technite.RelativeTarget.Invalid)
						{
							t.SetNextTask(Technite.Task.EatSurroundingCell, target);
						}
						else
							Out.Log(Significance.Unusual, "Unable to find adequate eating destination");
					}
				else
				{
					Out.Log(Significance.Unusual, "Insufficient resources to do anything");

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
