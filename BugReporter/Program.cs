using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BugReporter
{
	static class Program
	{
		[System.Runtime.InteropServices.DllImport("user32.dll")]
		private static extern bool SetProcessDPIAware();

		static Whoops whoops;

		public static void Abort(string problem)
		{
			if (whoops == null)
				whoops = new Whoops();
			whoops.Raise(problem);
		}

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
			try
			{
				if (Environment.OSVersion.Version.Major >= 6)
					SetProcessDPIAware();

				Application.EnableVisualStyles();
				Application.SetCompatibleTextRenderingDefault(false);
				Application.Run(new MainForm());
			}
			catch (Exception ex)
			{
				Abort(ex.Message);
			}
		}
	}
}
