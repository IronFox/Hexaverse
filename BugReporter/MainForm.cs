using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BugReporter
{
	public partial class MainForm : Form
	{
		/// <summary>
		/// Shared client. Uses the system's certificate validation (chain, host name)
		/// and also checks certificate revocation.
		/// </summary>
		static readonly HttpClient http = new HttpClient(new HttpClientHandler { CheckCertificateRevocationList = true })
		{
			Timeout = TimeSpan.FromMinutes(2),
		};

		static MainForm()
		{
			http.DefaultRequestHeaders.UserAgent.ParseAdd("HexaverseBugReporter/2.0");
		}

		readonly ReporterSettings settings = Program.Settings ?? ReporterSettings.Load();

		/// <summary>True while reports are uploaded to the configured server, false when they are saved to a file.</summary>
		bool uploadMode;

		public MainForm()
		{
			InitializeComponent();
			try
			{
				Icon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath);
			}
			catch
			{ }
			privacyNotice.Visible = settings.PrivacyNoticeUrl.Length > 0;
		}

		/// <summary>
		/// Posts the given fields, plus the configured token if any, to the configured report URL
		/// and returns the server's reply.
		/// </summary>
		async Task<string> PostAsync(IEnumerable<KeyValuePair<string, string>> fields)
		{
			var all = new List<KeyValuePair<string, string>>();
			if (settings.Token.Length > 0)
				all.Add(new KeyValuePair<string, string>("token", settings.Token));
			all.AddRange(fields);

			using var content = new FormUrlEncodedContent(all);
			using HttpResponseMessage response = await http.PostAsync(settings.UploadUri, content);
			response.EnsureSuccessStatusCode();
			return (await response.Content.ReadAsStringAsync()).Trim();
		}

		static KeyValuePair<string, string> Field(string name, string value) => new KeyValuePair<string, string>(name, value ?? "");

		void SetStatus(string text)
		{
			toolStripStatusLabel.Text = text.Replace("\n", " ").Replace("\r", "");
		}

		void UseSaveMode(string reason)
		{
			uploadMode = false;
			sendButton.Text = "Save report...";
			sendButton.Enabled = true;
			SetStatus(reason);
		}

		/// <summary>
		/// Without a configured report URL, reports are only saved to files. Otherwise the server
		/// is asked to respond first (up to three attempts), so the user knows whether sending will work.
		/// </summary>
		async void StartServerCheck()
		{
			if (settings.UploadUri == null)
			{
				UseSaveMode(settings.Problem ?? "No report server is configured. Save the report to a file and attach it to an issue on the project page.");
				return;
			}

			uploadMode = true;
			sendButton.Text = "Send";
			sendButton.Enabled = false;
			SetStatus("Contacting " + settings.UploadUri.Host + "...");
			string problem = null;
			for (int attempt = 0; attempt < 3; attempt++)
			{
				if (attempt > 0)
					await Task.Delay(1000);
				try
				{
					string reply = await PostAsync(new[] { Field("action", "respond") });
					if (reply == "ok")
					{
						SetStatus(settings.UploadUri.Host + " is ready to receive your report");
						sendButton.Enabled = true;
						return;
					}
					problem = "unexpected reply '" + reply + "'";
				}
				catch (Exception ex)
				{
					problem = ex.GetBaseException().Message;
				}
			}
			UseSaveMode("Unable to reach " + settings.UploadUri.Host + " (" + problem + "). You can save the report to a file instead.");
		}

		public IEnumerable<FileID> FetchLogFileList()
		{
			var fileList = GetFilesIn(new string[] { "client", "server", "aiClient" });

			foreach (var f in fileList)
				yield return f;

		}
		List<FileID> logFiles = new List<FileID>();
		enum CommandMode
		{
			None,
			Message,
		};


		private void MainForm_Load(object sender, EventArgs e)
		{
			try
			{
				ProcessStartInfo startInfo = new ProcessStartInfo(Path.Combine(AppContext.BaseDirectory, "..", "server", "Server.exe"));
				startInfo.CreateNoWindow = true;
				startInfo.UseShellExecute = false;
				startInfo.Arguments = "--version";
				startInfo.RedirectStandardOutput = true;
				startInfo.WorkingDirectory = Path.Combine(AppContext.BaseDirectory, "..", "server");
				Process server = Process.Start(startInfo);
				string version = server.StandardOutput.ReadToEnd();
				if (version.StartsWith("Hexaverse "))
					version = version.Substring(10);
				int at = version.IndexOf('[');
				if (at >= 0)
					version = version.Remove(at);
				gameVersion.Text = version.Trim();
			}
			catch
			{
				//MessageBox.Show("Error trying to launch 'server --version' to figure out the current game version. You will have to specify the game version by yourself: " + ex.Message);
				//gameVersion.Text = ex.ToString();
				//don't care, server not there, just leave empty
			}

			StartServerCheck();
			logFiles = FetchLogFileList().ToList();
			logFiles.Sort();


			CommandMode mode = CommandMode.None;

			foreach (var c in Environment.GetCommandLineArgs().Skip(2))
			{
				if (c.StartsWith("-"))
				{
					switch (c)
					{
						case "-m":
						case "--message":
							mode = CommandMode.Message;
							break;
					}
				}
				else
					switch (mode)
					{
						case CommandMode.Message:
							UpdateFatal(c);
							break;
					}

			}

			FinishLoading();
		}

		

		//IEnumerable<FileID> FilterLoadedFiles()
		//{

		//	foreach (var f in logFiles)
		//	{
		//		if (string.IsNullOrWhiteSpace(f.Version) || string.IsNullOrWhiteSpace(gameVersion.Text) || f.Version.StartsWith(gameVersion.Text) || gameVersion.Text.StartsWith(f.Version))
		//			yield return f;
		//	}
		//}



		public void FinishLoading(List<LogFile> logFiles)
		{
			//loadedFiles = logFiles;
			FinishLoading();
		}

		private void FinishLoading()
		{
			var fileList = logFiles;

			var items = logFile.Items;
			items.Clear();
			items.Add("None");

			foreach (var f in fileList)
				items.Add(f);

			if (logFile.SelectedItem == null)
				logFile.SelectedIndex = 0;



			{
				string[] args = Environment.GetCommandLineArgs();
				if (args.Length > 1 && !args[1].StartsWith("-"))
				{
					try
					{
						string filename = args[1];
						if (filename.StartsWith("\\\\?\\"))
							filename = filename.Substring(4);
						{
							FileID? file = null;
							foreach (var obj in logFile.Items)
							{
								if (!(obj is FileID))
									continue;
								FileID candidate = (FileID)obj;
								if (candidate.Path == filename)
								{
									file = candidate;
									logFile.SelectedItem = candidate;
									break;
								}
							}
						}
					}
					catch
					{ }
				}
			}
		}






		private IEnumerable<FileID> GetFilesIn(string[] groups)
		{
			string path = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
			List<LogFile> fileList = new List<LogFile>();
			foreach (var group in groups)
			{
				string logsAt = Path.Combine(path, "My Games", "Hexaverse", group, "logs");
				string[] files = null;
				try
				{
					files = Directory.GetFiles(logsAt);
				}
				catch (Exception ex)
				{
					continue;
					//MessageBox.Show("Error accessing '"+ logsAt+"': "+ex.Message);
				}
				if (files != null)
					foreach (string f in files)
					{
						yield return new FileID(group, f);
					}
			}
		}

		Queue<FileID> timedLoadQueue = new Queue<FileID>();

		void QueueFront(FileID f)
		{
			var items = timedLoadQueue.ToArray();
			timedLoadQueue.Clear();
			timedLoadQueue.Enqueue(f);
			foreach (var item in items)
				timedLoadQueue.Enqueue(item);
		}


		private string lastFatal = null;

		private void UpdateFatal(string newFatal)
		{
			if (lastFatal != newFatal)
			{
				int foundAt = -1;
				if (lastFatal != null)
				{
					int at;
					while ((at = bugDescription.Text.IndexOf(lastFatal)) != -1)
					{
						foundAt = at;
						bugDescription.Text = bugDescription.Text.Remove(at, lastFatal.Length);
					}
					lastFatal = null;
				}

				if (newFatal != null)
				{
					if (foundAt >= 0)
						bugDescription.Text = bugDescription.Text.Insert(foundAt, newFatal);
					else
					{
						if (string.IsNullOrWhiteSpace(bugDescription.Text))
							bugDescription.Text = newFatal;
						else
							bugDescription.Text = newFatal + "\r\n" + "\r\n" + bugDescription.Text.Trim(new char[] { '\r', '\n' });
					}
					bugDescription.Select(0, 0);
					lastFatal = newFatal;
				}
			}

		}


		private void logFile_SelectedIndexChanged(object sender, EventArgs e)
		{
			var items = logFileList.Items;
			items.Clear();
			timedLoadQueue.Clear();


			try
			{
				if (logFile.SelectedItem == null || !(logFile.SelectedItem is FileID))
					return;
				FileID f = (FileID)logFile.SelectedItem;
				gameVersion.Text = f.Version;

				UpdateFatal(f.Fatal);

				LogFile root = f.CachedFile;
				if (root == null)
				{
					if (!timedLoadQueue.Contains(f))
						QueueFront(f);
				}
				else
				{
					List<LogFile> matches = new List<LogFile>();

					foreach (var obj in logFile.Items)
					{
						if (!(obj is FileID))
							continue;
						FileID candidate = (FileID)obj;
						if (candidate.Path == f.Path)
							continue;

						LogFile check = candidate.CachedFile;
						if (check == null)
						{
							if (!timedLoadQueue.Contains(candidate))
								timedLoadQueue.Enqueue(candidate);
							continue;
						}

						string found = check.ScanIDs(id => { if (root.ContainsEntityID(id)) return true; return false; });
						if (found != null)
							matches.Add(check);
					}
					matches.Sort();
					matches.Reverse();

					foreach (var m in matches)
						items.Add(m, true);
				}
			}
			catch (Exception ex)
			{
				Debug.Fail(ex.ToString());
			}
		}



		private void abortButton_Click(object sender, EventArgs e)
		{
			Application.Exit();
		}


		private void ActivateInput(bool enable)
		{
			bugDescription.Enabled = enable;
			reproductionDesc.Enabled = enable;
			gameVersion.Enabled = enable;
			logFile.Enabled = enable;
			logFileList.Enabled = enable;
			sendButton.Enabled = enable;
		}

		private void MainForm_Shown(object sender, EventArgs e)
		{
			BringToFront();
			TopMost = true;
		}

		Task<LogFile> loadTask = null;

		private void loadTimer_Tick(object sender, EventArgs e)
		{
			if (loadTask != null)
			{
				if (loadTask.IsCanceled || loadTask.IsCompleted || loadTask.IsFaulted)
				{
					try
					{
						var t = loadTask.Result;    //dunno if necessary. let crash if crash
					}
					catch
					{ }
					loadTask = null;
					logFile_SelectedIndexChanged(sender, e);	//trigger check
				}
				else
					return;
			}

			if (timedLoadQueue.Count == 0)
				return;
			loadTask = timedLoadQueue.Dequeue().LoadFileAsync();
		}




		private void logFileList_MouseUp(object sender, MouseEventArgs e)
		{
			if (logFileList.SelectedItem is LogFile && e.Button == MouseButtons.Right)
			{
				OpenInDefaultViewer(((LogFile)logFileList.SelectedItem).File);
			}
		}

		private void OpenInDefaultViewer(FileID file)
		{
			TopMost = false;
			file.OpenInDefaultViewer();
		}

		private void viewPrimaryButton_Click(object sender, EventArgs e)
		{
			if (logFile.SelectedItem is FileID)
			{
				OpenInDefaultViewer((FileID)logFile.SelectedItem);
			}
		}

		private void privacyNotice_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			TopMost = false;
			Program.OpenExternally(settings.PrivacyNoticeUrl);
		}

		/// <summary>The selected primary log file, if any, and the checked related ones.</summary>
		async Task<(LogFile primary, List<LogFile> related)> CollectLogsAsync()
		{
			LogFile primary = null;
			var related = new List<LogFile>();
			if (logFile.SelectedItem is FileID id)
			{
				primary = await id.LoadFileAsync();
				foreach (var item in logFileList.CheckedItems)
					related.Add((LogFile)item);
			}
			return (primary, related);
		}

		private async void sendButton_Click(object sender, EventArgs e)
		{
			if (bugDescription.Text.Trim().Length == 0)
			{
				MessageBox.Show("Please describe the problem in the top-most text field. Thank you");
				return;
			}
			ActivateInput(false);
			try
			{
				if (uploadMode)
					await UploadAsync();
				else
					await SaveAsync();
			}
			catch (Exception ex)
			{
				Program.Abort(ex.Message);
			}
		}

		async Task UploadAsync()
		{
			var (primary, related) = await CollectLogsAsync();
			var fields = new List<KeyValuePair<string, string>>
			{
				Field("action", "store"),
				Field("description", bugDescription.Text),
				Field("reproduction", reproductionDesc.Text),
				Field("contact", whoAmI.Text),
				Field("gameVersion", gameVersion.Text),
			};
			if (primary != null)
			{
				if (primary.Fatal != null)
					fields.Add(Field("fatalMessage", primary.Fatal));
				fields.Add(Field("primaryLogFileName", primary.ToString()));
				fields.Add(Field("primaryLogFileData", Convert.ToBase64String(primary.Compress())));
				foreach (var f in related)
				{
					fields.Add(Field("logFileName[]", f.ToString()));
					fields.Add(Field("logFileData[]", Convert.ToBase64String(f.Compress())));
				}
			}

			SetStatus("Uploading...");
			string reply;
			try
			{
				reply = await PostAsync(fields);
			}
			catch (Exception ex)
			{
				ActivateInput(true);
				UseSaveMode("Unable to upload to " + settings.UploadUri.Host + " (" + ex.GetBaseException().Message + "). You can save the report to a file instead.");
				return;
			}
			if (reply == "ok")
			{
				MessageBox.Show("Upload succeeded. Thank you!");
				Application.Exit();
				return;
			}
			SetStatus("Server said something unexpected: '" + reply + "'");
			ActivateInput(true);
		}

		/// <summary>
		/// Writes the description and the selected log files into a .zip the user can attach to an issue.
		/// </summary>
		async Task SaveAsync()
		{
			var (primary, related) = await CollectLogsAsync();

			using var dialog = new SaveFileDialog
			{
				Title = "Save bug report",
				Filter = "Zip archive (*.zip)|*.zip",
				FileName = "Hexaverse bug report " + DateTime.Now.ToString("yyyy-MM-dd HHmmss") + ".zip",
				InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.Desktop),
			};
			TopMost = false;
			if (dialog.ShowDialog(this) != DialogResult.OK)
			{
				ActivateInput(true);
				return;
			}

			var report = new StringBuilder();
			report.AppendLine("Hexaverse bug report");
			report.AppendLine("Created: " + DateTime.Now.ToString("u"));
			report.AppendLine("Game version: " + gameVersion.Text);
			if (whoAmI.Text.Trim().Length > 0)
				report.AppendLine("Contact: " + whoAmI.Text.Trim());
			if (primary?.Fatal != null)
				report.AppendLine().AppendLine("Fatal error:").AppendLine(primary.Fatal);
			report.AppendLine().AppendLine("What happened:").AppendLine(bugDescription.Text.Trim());
			if (reproductionDesc.Text.Trim().Length > 0)
				report.AppendLine().AppendLine("What happened before:").AppendLine(reproductionDesc.Text.Trim());

			await Task.Run(() =>
			{
				using var zip = ZipFile.Open(dialog.FileName, ZipArchiveMode.Create);
				using (var writer = new StreamWriter(zip.CreateEntry("report.txt").Open(), new UTF8Encoding(false)))
					writer.Write(report.ToString());
				var added = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
				foreach (var log in new[] { primary }.Concat(related))
				{
					if (log == null)
						continue;
					string name = log.File.Group + "/" + Path.GetFileName(log.File.Path);
					if (!added.Add(name))
						continue;
					using var source = new FileStream(log.File.Path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
					using var target = zip.CreateEntry(name, CompressionLevel.Optimal).Open();
					source.CopyTo(target);
				}
			});

			string message = "The report was saved to\n" + dialog.FileName;
			if (settings.ProjectUrl.Length > 0)
			{
				message += "\n\nWould you like to open " + settings.ProjectUrl + " to report the problem there? Please attach the saved file.";
				if (MessageBox.Show(message, "Bug report saved", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
					Program.OpenExternally(settings.ProjectUrl);
			}
			else
				MessageBox.Show(message, "Bug report saved", MessageBoxButtons.OK, MessageBoxIcon.Information);
			Application.Exit();
		}
	}
}
