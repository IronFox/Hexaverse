using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ServerManager
{
	static class Program
	{
		public static string gameVersion;
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
			while (!File.Exists(Path.Combine("server", "Server.exe")))
			{
				try
				{
					Directory.SetCurrentDirectory(new DirectoryInfo(Directory.GetCurrentDirectory()).Parent.FullName);
				}
				catch
				{
					MessageBox.Show("Unable to locate Hexaverse working directory", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
					return;
				}
			}


			try
			{
				var info = new ProcessStartInfo();
				info.UseShellExecute = false;
				info.RedirectStandardOutput = true;
				info.CreateNoWindow = true;
				info.FileName = Path.Combine("server", "Server.exe");
				info.Arguments = "--version";
				var p = Process.Start(info);
				p.WaitForExit();
				gameVersion = p.StandardOutput.ReadToEnd();
			}
			catch (Exception ex)
			{
				gameVersion = ex.Message;
			}

			try
			{
				NiceName.Load();
				Application.EnableVisualStyles();
				Application.SetCompatibleTextRenderingDefault(false);
				Application.Run(new ServerTable());
			}
			catch (Exception ex)
			{
				Debug.Fail(ex.ToString());
				//call bug reporter (later)
			}
		}
	}
}
