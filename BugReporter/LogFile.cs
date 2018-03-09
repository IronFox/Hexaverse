using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BugReporter
{


	public struct FileID : IComparable<FileID>
	{
		public readonly string Path;
		public readonly string Group;
		/// <summary>
		/// Version determined from file path.
		/// May be null, if the path does not contain a version
		/// </summary>
		public readonly string Version;
		public readonly string LastLine, Fatal;
		public readonly DateTime CreationTime,
								LastLineTime;
//		public readonly long FileSize;
		public readonly int NumLines;
			

		public FileID(string group, string path)
		{
			Path = path;
			Group = group;
			var info = new FileInfo(path);
			LastLineTime = info.LastWriteTime;

			string[] parts = info.Name.Split('.');
			if (parts.Length > 5 && parts[parts.Length - 5].StartsWith("v"))
				Version =  string.Join(".",  parts.Skip(parts.Length - 5).Take(4));
			else
				Version = null;

			CreationTime = info.CreationTime;
			//client 2018 03 01 143749
			if (parts.Length > 0)
			{
				parts = parts[0].Split(' ');
				if (parts.Length > 3)
				{
					try
					{
						var year = int.Parse( parts[parts.Length - 4] );
						var month = int.Parse(parts[parts.Length - 3]);
						var day = int.Parse(parts[parts.Length - 2]);
						var time = parts[parts.Length - 1];
						if (time.Length == 6)
						{
							var hour = int.Parse(time.Substring(0, 2));
							var minute = int.Parse(time.Substring(2, 2));
							var second = int.Parse(time.Substring(4, 2));

							CreationTime = new DateTime(year, month, day, hour, minute, second);

						}
					}
					catch 
					{ }	//really don't care

				}
			}



			Fatal = null;
			NumLines = 0;
			LastLine = null;
			reference = null;
			try
			{
				foreach (var l in File.ReadLines(path))
				{
					LastLine = l;
					NumLines++;

					if (Fatal == null && l.StartsWith("SystemFatal"))
					{
						parts = l.Split('\t');
						if (parts.Length > LogFile.MessageAt && parts[0] == "SystemFatal" && parts[LogFile.MessageAt].StartsWith("Fatal exception"))
						{
							Fatal = parts[LogFile.MessageAt];
						}
					}
				}
				parts = LastLine.Split('\t');

				DateTime t2;
				if (parts.Length > 1 && LogFile.TryParseTime(parts[1], out t2))
					LastLineTime = t2;
				
				if (parts.Length > LogFile.MessageAt && parts[0] == "SystemFatal" && parts[LogFile.MessageAt].StartsWith("Fatal exception"))
				{
					Fatal = parts[LogFile.MessageAt];
				}

			}
			catch
			{
				LastLine = null;
			}
		}

		public string TimeRange
		{
			get
			{
				if (CreationTime.Date == LastLineTime.Date)
				{
					if (CreationTime.Date == DateTime.Now.Date)
						return "[" + CreationTime.ToShortTimeString() + "-" + LastLineTime.ToShortTimeString() + "]";
					return "[" + CreationTime.Date.ToShortDateString() + " " + CreationTime.ToShortTimeString() + "-" + LastLineTime.ToShortTimeString() + "]";
				}
				else
					return "[" + CreationTime + "-" + LastLineTime + "]";
			}
		}

		private LogFile reference;

		public LogFile CachedFile
		{
			get
			{
				if (reference == null)
					reference = LogFile.GetCached(this);
				return reference;
			}
		}

		public async Task<LogFile> LoadFileAsync()
		{
			if (reference == null)
				reference = await LogFile.LoadAsync(this);
			return reference;
		}

		public override string ToString()
		{
			var r = CachedFile;
			if (r != null)
				return r.ToString();
			string rs = TimeRange + ": "+Group + " " + Version + " [" + NumLines  + " lines]";
			if (Fatal != null)
				rs += " (Reports Fatal Exception)";
			return rs;
		}

		public override bool Equals(object obj)
		{
			if (!(obj is FileID))
				return false;
			return Path == ((FileID)obj).Path;
		}

		public override int GetHashCode()
		{
			return Path.GetHashCode();
		}

		public int CompareTo(FileID other)
		{
			return -LastLineTime.CompareTo(other.LastLineTime);
		}

		//https://stackoverflow.com/questions/12744725/how-do-i-perform-file-readalllines-on-a-file-that-is-also-open-in-excel
		/// <summary>
		/// Reads all lines from the local file, while it may also be open for writing
		/// </summary>
		/// <returns>Read lines</returns>
		public IEnumerable<string> ReadAllLines()
		{
			using (var csv = new FileStream(Path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
			using (var sr = new StreamReader(csv))
			{
				while (!sr.EndOfStream)
					yield return sr.ReadLine();
			}
		}


	}

	public class LogFile : IComparable
	{
		public readonly FileID File;
		public DateTime MinTime;
		public DateTime MaxTime;
		private HashSet<string> entityIDs;
		private string[][] lines;
		public string Combined { get; private set; }

		private static Dictionary<string, LogFile> cache = new Dictionary<string, LogFile>();


		public static LogFile GetCached(FileID id)
		{
			LogFile rs;
			if (cache.TryGetValue(id.Path, out rs))
				return rs;
			return null;
		}

		public static async Task<LogFile> LoadAsync(FileID file)
		{
			LogFile rs;
			if (cache.TryGetValue(file.Path, out rs))
				return rs;
			rs = new LogFile(file);
			cache[file.Path] = rs;
			await rs.LoadAsync();
			return rs;
		}


		public string Fatal
		{
			get
			{
				return File.Fatal;
			}
		}

		public bool ContainsEntityID(string id)
		{
			Load();
			return entityIDs.Contains(id);
		}



		private void LoadLines()
		{
			if (lines == null)
			{
				try
				{
					StringBuilder combined = new StringBuilder();
					var lines0 = File.ReadAllLines();
					//lines = new string[lines0.Length][];
					List<string[]> tempLines = new List<string[]>();
					foreach (var line in lines0)
					{
						tempLines.Add(line.Split('\t'));
						combined.AppendLine(line);
					}
					lines = tempLines.ToArray();
					if (combined.Length > 65535)
						combined.Remove(0, combined.Length - 65535);
					Combined = combined.ToString();
				}
				catch { }

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
		public const int MessageAt = 10;

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

		private static string Get(string[] ar, int index)
		{
			if (index < ar.Length)
				return ar[index];
			return "";
		}

		private static DateTime Min(DateTime a, DateTime b)
		{
			return new DateTime(Math.Min(a.Ticks, b.Ticks));
		}
		private static DateTime Max(DateTime a, DateTime b)
		{
			return new DateTime(Math.Max(a.Ticks, b.Ticks));
		}

		public string TimeRange
		{
			get
			{
				if (MinTime.Date == MaxTime.Date)
				{
					return "[" + MinTime.Date.ToShortDateString() + " " + MinTime.ToShortTimeString() + "-" + MaxTime.ToShortTimeString() + "]";
				}
				else
					return "[" + MinTime + "-" + MaxTime + "]";
			}
		}


		private Task LoadAsync()
		{
			return Task.Run(() => Load());
		}

		public static bool TryParseTime(string dt, out DateTime t)
		{
			t = DateTime.MinValue;
			try
			{
				int dotAt = dt.IndexOf('.');
				if (dotAt < 0)
					return false;
				string date = dt.Substring(0, dotAt);
				string time = dt.Substring(dotAt + 1).Trim();
				string[] dateParts = date.Split(' ');
				int year = int.Parse(Get(dateParts, 0));
				int month = int.Parse(Get(dateParts, 1));
				int day = int.Parse(Get(dateParts, 2));
				t = new DateTime(year, month, day);
				string[] timeParts = time.Split(':');
				int hour = int.Parse(Get(timeParts, 0));
				int minute = int.Parse(Get(timeParts, 1));
				int seconds = int.Parse(Get(timeParts, 2));
				t += new TimeSpan(hour, minute, seconds);
				return true;
			}
			catch
			{
			}
			return false;
		}

		private void Load()
		{
			if (entityIDs != null)
				return;
			entityIDs = new HashSet<string>();
			const int idAt = 7;

			MinTime = DateTime.MaxValue;
			MaxTime = DateTime.MinValue;


			VisitLines(line =>
			{
				if (line.Length > 1)
				{
					DateTime t;
					string dt = line[1];
					if (TryParseTime(dt,out t) )
					{
						MinTime = Min(MinTime, t);
						MaxTime = Max(MinTime, t);
					}
				}
				if (line.Length > idAt)
				{
					string id = line[idAt].Trim();
					if (id.Length == 0 || id == "EntityID")
						return;
					entityIDs.Add(id);
				}
			});
		}


		public LogFile(FileID id)
		{
			File = id;
		}



		public int CompareTo(object obj)
		{
			LogFile other = (LogFile)obj;
			return MaxTime.CompareTo(other.MaxTime);
		}

		public override string ToString()
		{
			string rs = TimeRange + ": " + File.Group + " " + File.Version + " [" + File.NumLines + " lines]";
			if (File.Fatal != null)
				rs += " (Reports Fatal Exception)";
			return rs;
		}
	}
}
