using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace TechniteLogic
{
	public static class Interface
	{

		public static Protocol.OutChannel<Protocol.Interface.TechniteInstructionChunk> techniteInstructionChunk = new Protocol.OutChannel<Protocol.Interface.TechniteInstructionChunk>(Protocol.ChannelID.TechniteInstructionChunk);

	    public static void Register()
        {
			Protocol.ChannelMap.Register<Protocol.Interface.TechniteStateChunk>(Protocol.ChannelID.TechniteStateChunk, Event.TechniteStateChunk);
			//Protocol.ChannelMap.Register<string>(Protocol.ChannelID.NewAccount, Event.NewAccount,false);
			Protocol.ChannelMap.RegisterSignal(Protocol.ChannelID.InstructTechnites,Event.InstructTechnites);
		}


		public class Struct
		{
		
		}
		public static class Event
		{

			public static void InstructTechnites(Protocol.Client cl)
			{
				Technite.Cleanup();	//updates must be done by now
				Logic.ProcessTechnites();

				int numTechnites = Technite.Count;
				int numChunks = numTechnites / Protocol.Interface.TechniteInstructionChunk.MaxPerChunk;
				if ((numTechnites % Protocol.Interface.TechniteInstructionChunk.MaxPerChunk)!= 0)
					numChunks++;

				var e = Technite.All.GetEnumerator();
				int offset = 0;
				for (int i = 0; i < numChunks; i++)
				{
					int chunkSize = Math.Min(Protocol.Interface.TechniteInstructionChunk.MaxPerChunk, numTechnites - offset);

					Protocol.Interface.TechniteInstructionChunk chunk = new Protocol.Interface.TechniteInstructionChunk();
					chunk.Offset = (uint)offset;
					chunk.Instructions = new Protocol.Interface.TechniteInstruction[chunkSize];
					for (int j = 0; j < chunkSize; j++)
					{
						Technite t = e.Current;
						chunk.Instructions[j] = t.ExportInstruction();
						bool success = e.MoveNext();
						Debug.Assert(success || (i+1 == numChunks && j+1== chunkSize));
					}

					techniteInstructionChunk.SendTo(cl,chunk);

					offset += chunkSize;
				}
			}

			public static void TechniteStateChunk(Protocol.Client cl, Protocol.Interface.TechniteStateChunk chunk)
			{
				if (chunk.IsFirst)
					Technite.Reset();
				foreach (var state in chunk.States)
					Technite.CreateOrUpdate(state);

			}
		}
	}
}
