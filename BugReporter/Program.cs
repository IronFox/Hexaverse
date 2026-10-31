using System;
using System.Diagnostics;
using System.Windows.Forms;

namespace BugReporter
{
	static class Program
	{
		static Whoops whoops;

		public static ReporterSettings Settings { get; private set; }

		public static void Abort(string problem)
		{
			whoops ??= new Whoops();
			whoops.Raise(problem);
		}

		/// <summary>Opens a URL or file with its associated application.</summary>
		public static void OpenExternally(string target)
		{
			if (string.IsNullOrWhiteSpace(target))
				return;
			Process.Start(new ProcessStartInfo(target) { UseShellExecute = true });
		}

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
			try
			{
				ApplicationConfiguration.Initialize();
				Settings = ReporterSettings.Load();
				Application.Run(new MainForm());
			}
			catch (Exception ex)
			{
				Abort(ex.Message);
			}
		}
	}
}
