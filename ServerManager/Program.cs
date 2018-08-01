using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ServerManager
{
	static class Program
	{
		[System.Runtime.InteropServices.DllImport("user32.dll")]
		private static extern bool SetProcessDPIAware();

		public static string gameVersion;
		public static string gameDir;
		//public static string installDir;	//Directory.GetCurrentDirectory();
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
			{
				//https://social.msdn.microsoft.com/Forums/en-US/76409b4b-58e7-4d83-9257-6bfaadf53a87/how-to-start-only-once-an-application?forum=devdocs
				bool isNew;
				Mutex mutex = new Mutex(true, Application.ProductName, out isNew);
				if (!isNew)
					return;
			}

			while (!File.Exists(Path.Combine("server", "Server.exe")))
			{
				try
				{
					var current = new DirectoryInfo(Directory.GetCurrentDirectory());
					if (current.Parent == null)
					{
						MessageBox.Show("Unable to locate Hexaverse working directory", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
						return;
					}
					Directory.SetCurrentDirectory(current.Parent.FullName);
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
				if (Environment.OSVersion.Version.Major >= 6)
					SetProcessDPIAware();

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

		public static void ResizeThis(Control c, float scaleFactor)
		{
			c.Font = new Font(c.Font.FontFamily.Name, c.Font.Size * scaleFactor);
		}
		public static void ResizeFont(Control.ControlCollection coll, float scaleFactor)
		{
			foreach (Control c in coll)
			{
				if (c.HasChildren)
				{
					ResizeFont(c.Controls, scaleFactor);
				}
				ResizeThis(c, scaleFactor);
				c.Height = c.PreferredSize.Height;
			}
		}


		public static Process StartComponent(string path, string executableBaseName, string parameters = "", bool hideWindow=false)
		{
			ProcessStartInfo info = new ProcessStartInfo();
			info.Arguments = parameters;
			info.CreateNoWindow = hideWindow;
			if (hideWindow)
				info.UseShellExecute = false;
			path = Path.Combine(Directory.GetCurrentDirectory(), path);
			info.FileName = Environment.Is64BitOperatingSystem ? Path.Combine(path, executableBaseName + ".exe") : Path.Combine(path, executableBaseName+ " x86.exe");
			info.WorkingDirectory = path;
			return Process.Start(info);
		}

		public static void RunComponent(string path, string executableBaseName, string parameters = "", bool hideWindow = false)
		{
			using (StartComponent(path, executableBaseName, parameters,hideWindow))
			{}
		}
	}
}
