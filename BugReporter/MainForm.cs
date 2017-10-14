using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BugReporter
{
	public partial class MainForm : Form
	{
		class LogFile : IComparable
		{
			public readonly string Path;
			public readonly string TimeLine, Prefix;
			public DateTime Created;
			public DateTime Modified;
			private HashSet<string> entityIDs;
			private string fatal;
			private string[][] lines;
			public string Version { get; private set; }

			public string Fatal
			{
				get
				{
					return fatal;
				}
			}

			public bool ContainsEntityID(string id)
			{
				Load();
				return entityIDs.Contains(id);
			}

			public void LoadLines()
			{
				if (lines == null)
				{
					string[] lines0 = File.ReadAllLines(Path);
					lines = new string[lines0.Length][];
					for (int i = 0; i < lines0.Length; i++)
						lines[i] = lines0[i].Split('\t');
				}
			}

			public string[] ScanLines(Func<string[], bool> f)
			{
				LoadLines();
				foreach (var line in lines)
					if (f(line))
						return line;
				return null;
			}

			public void VisitLines(Action<string[]> f)
			{
				LoadLines();
				foreach (var line in lines)
					f(line);
			}
			private const int messageAt = 10;

			public void VisitIDs(Action<string> f)
			{
				Load();
				foreach (var e in entityIDs)
					f(e);
			}

			public string ScanIDs(Func<string, bool> f)
			{
				Load();
				foreach (var e in entityIDs)
					if (f(e))
						return e;
				return null;
			}

			public void Load()
			{
				if (entityIDs != null)
					return;
				entityIDs = new HashSet<string>();
				const int idAt = 7;
				VisitLines(line =>
				{
					if (line.Length > idAt)
					{
						string id = line[idAt].Trim();
						if (id.Length == 0 || id == "EntityID")
							return;
						entityIDs.Add(id);
					}
				});
				VisitLines(line =>
				{
					if (line.Length <= messageAt)
						return;
					string msg = line[messageAt];
					if (line[0] == "SystemFatal" && msg.StartsWith("Fatal exception"))
					{
						fatal = line[messageAt];
						return;
					}

					if (msg.StartsWith("Starting "))
					{
						string[] words = msg.Split(' ');
						if (words.Length == 3)
						{
							Version = words[2];
						}
					}
				});


			}


			public LogFile(string path)
			{
				Path = path;
				Created = File.GetCreationTime(path);
				Modified = File.GetLastWriteTime(path);

				string filename = System.IO.Path.GetFileNameWithoutExtension(path);
				Prefix = filename.Split(' ')[0];
				if (Created.Date == Modified.Date)
				{
					TimeLine = "[" +Created.Date.ToShortDateString() + " " + Created.ToShortTimeString() + "-" + Modified.ToShortTimeString() + "]";
				}
				else
					TimeLine = "[" + Created + "-" + Modified + "]";

				Load();
			}

			public int CompareTo(object obj)
			{
				LogFile other = (LogFile)obj;
				return Modified.CompareTo(other.Modified);
			}

			public override string ToString()
			{
				string rs = Prefix;
				if (Version != null)
					rs += ' ' + Version;
				rs += ' '+TimeLine;
				if (fatal != null)
					rs += " (reports fatal exception)";
				return rs;
			}
		}


		public MainForm()
		{
			InitializeComponent();
		}

		private void MainForm_Load(object sender, EventArgs e)
		{
			logGroup.SelectedIndex = 0;

			try
			{
				ProcessStartInfo startInfo = new ProcessStartInfo(new FileInfo(Path.Combine(Directory.GetCurrentDirectory(), "..", "server", "server.exe")).FullName);
				startInfo.CreateNoWindow = true;
				startInfo.UseShellExecute = false;
				startInfo.Arguments = "--version";
				startInfo.RedirectStandardOutput = true;
				Process server = Process.Start(startInfo);
				string version = server.StandardOutput.ReadToEnd();
				if (version.StartsWith("Hexaverse "))
					version = version.Substring(10);
				gameVersion.Text = version;
			}
			catch (Exception ex)
			{
				//gameVersion.Text = ex.ToString();
			}
		}

		private void logGroup_SelectedIndexChanged(object sender, EventArgs e)
		{
			var items = logFile.Items;
			items.Clear();

			if (logGroup.SelectedIndex <= 0)
			{
				logFileList.Items.Clear();
				return;
			}
			try
			{
				List<LogFile> fileList = GetFilesIn(logGroup.SelectedItem.ToString().ToLower(),gameVersion.Text);


				foreach (LogFile f in fileList)
				{
					items.Add(f);
				}
				if (items.Count > 0)
					logFile.SelectedItem = fileList[0];
			}
			catch (Exception)
			{ }


		}

		private static List<LogFile> GetFilesIn(string group, string gameVersion)
		{
			string path = System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
			string logsAt = Path.Combine(path, "My Games", "Hexaverse", group, "logs");
			string[] files = Directory.GetFiles(logsAt);

			List<LogFile> fileList = new List<LogFile>();
			foreach (string f in files)
			{
				LogFile file = new LogFile(f);
				if (file.Version == null || !gameVersion.StartsWith(file.Version))
					continue;
				fileList.Add(file);
			}

			fileList.Sort();
			fileList.Reverse();
			return fileList;
		}


		

		private void logFile_SelectedIndexChanged(object sender, EventArgs e)
		{
			var items = logFileList.Items;
			items.Clear();
			try
			{
				if (logFile.SelectedItem == null)
					return;
				LogFile f = (LogFile)logFile.SelectedItem;




				string opposite = logGroup.SelectedItem.ToString().ToLower() == "server" ? "client" : "server";
				List<LogFile> others = GetFilesIn(opposite, gameVersion.Text);
				List<LogFile> matches = new List<LogFile>();

				foreach (var candidate in others)
				{
					string found = candidate.ScanIDs(id => { if (f.ContainsEntityID(id)) return true; return false; });
					if (found != null)
						matches.Add(candidate);
				}
				matches.Sort();
				matches.Reverse();

				foreach (var m in matches)
					items.Add(m,true);
			}
			catch (Exception ex)
			{
				Debug.Fail(ex.ToString());
			}
		}

		private void logInfoLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			new Privacy_Policy().ShowDialog();
		}
	}
}
