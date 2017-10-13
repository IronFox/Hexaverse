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
		struct LogFile : IComparable
		{
			public string Path;
			public string Name;
			public DateTime Created;
			public DateTime Modified;

			public LogFile(string f, DateTime created, DateTime modified) : this()
			{
				Path = f;
				Created = created;
				Modified = modified;

				string filename = System.IO.Path.GetFileNameWithoutExtension(f);
				string prefix = filename.Split(' ')[0];
				if (Created.Date == Modified.Date)
				{
					Name = prefix+ " [" +Created.Date.ToShortDateString() + " " + Created.ToShortTimeString() + " - " + Modified.ToShortTimeString() + "]";
				}
				else
					Name = prefix + " [" + Created + "-" + Modified + "]";
			}

			public int CompareTo(object obj)
			{
				LogFile other = (LogFile)obj;
				return Modified.CompareTo(other.Modified);
			}

			public override string ToString()
			{
				return Name;
			}
		}


		public MainForm()
		{
			InitializeComponent();
		}

		private void MainForm_Load(object sender, EventArgs e)
		{
			logGroup.SelectedIndex = 0;
		}

		private void logGroup_SelectedIndexChanged(object sender, EventArgs e)
		{
			var items = logFile.Items;
			items.Clear();

			if (logGroup.SelectedIndex <= 0)
				return;
			try
			{
				List<LogFile> fileList = GetFilesIn(logGroup.SelectedItem.ToString().ToLower());


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

		private static List<LogFile> GetFilesIn(string group)
		{
			string path = System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
			string logsAt = Path.Combine(path, "My Games", "Hexaverse", group, "logs");
			string[] files = Directory.GetFiles(logsAt);

			List<LogFile> fileList = new List<LogFile>();
			foreach (string f in files)
			{
				var created = File.GetCreationTime(f);
				var modified = File.GetLastWriteTime(f);
				fileList.Add(new LogFile(f, created, modified));
			}

			fileList.Sort();
			fileList.Reverse();
			return fileList;
		}

		private static void VisitIDsOf(string path, Action<string> action)
		{
			string[] lines = File.ReadAllLines(path);
			foreach (var line in lines)
			{
				string[] elements = line.Split('\t');
				if (elements.Length > 7)
				{
					string id = elements[7].Trim();
					if (id.Length == 0 || id == "EntityID")
						continue;
					action(id);
				}
			}
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

				HashSet<string> ids = new HashSet<string>();
				VisitIDsOf(f.Path, id => ids.Add(id));



				string opposite = logGroup.SelectedItem.ToString().ToLower() == "server" ? "client" : "server";
				List<LogFile> others = GetFilesIn(opposite);
				List<LogFile> matches = new List<LogFile>();

				foreach (var candidate in others)
				{
					bool match = false;
					VisitIDsOf(candidate.Path, id => { if (match || ids.Contains(id)) match = true; });
					if (match)
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
	}
}
