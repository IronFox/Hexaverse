using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Logging
{
	public enum Significance
	{
		Low,
		Common,
		Unusual,
		Important,
		ClientFatal,
		ProgramFatal
	}

    public static class Out
    {


		static Logger logger = new Logger();



		private static object logLock = new object();

		public static void Log(Significance significance, String message)
		{
			Log("","","", significance,message);

		}

		public static void Log(string clientName, string accountID, string accountName, Significance significance, String message)
		{
			string printLine, fileLine;
			fileLine = DateTime.Now.ToString();
			fileLine += "\t" + significance;

			fileLine += "\t" + clientName;

			fileLine += "\t" + accountID + "\t" + accountName;

			{
				fileLine += "\t" + "\t" + "\t";
				printLine = System.String.Format("[{1}]<{0}> {2}({3}): {4}", significance, DateTime.Now, clientName, accountName, message);
			}

			fileLine += "\t" + message;

			logger.SendEvent(fileLine);

			if (significance >= Significance.Unusual)
			{
				lock (logLock)
					System.IO.File.AppendAllText("execution.log", fileLine + "\n");
				Console.WriteLine(printLine);
			}
		}

		public static void Start()
		{
			logger.Start();
		}

		public static void Close()
		{
			ReportProvider.End();
			foreach (ReportProvider p in ReportProvider.All)
			{
				p.Close();
			}

		}
	}
}
