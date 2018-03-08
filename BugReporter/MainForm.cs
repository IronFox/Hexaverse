using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BugReporter
{
	public partial class MainForm : Form
	{
		public static readonly Uri ServerUri = new Uri("https://www.hexaverse.net/bugReport/store.php");
		//public static readonly Uri ServerUri = new Uri("https://www.hexaverse_nope.net/bugReport/store.php");



		public MainForm()
		{
			InitializeComponent();
		}

		static void PutParam(StringBuilder builder, string name, string value)
		{
			if (builder.Length > 0)
				builder.Append('&');
			EscapeDataString(builder, name);
			builder.Append('=');
			EscapeDataString(builder, value);
		}

		private static void EscapeDataString(StringBuilder builder, string str)
		{
			int maxCharacters = 2000;

			int numFullSteps = str.Length / maxCharacters;

			int numTotalSteps = (str.Length + maxCharacters - 1) / maxCharacters;

			for (int i = 0; i < numFullSteps; i++)
				builder.Append(Uri.EscapeDataString(str.Substring(maxCharacters * i, maxCharacters)));
			if (numTotalSteps > numFullSteps)
				builder.Append(Uri.EscapeDataString(str.Substring(maxCharacters * numFullSteps)));

		}

		static async Task<string> ContactServerAsync(IEnumerable<Tuple<string, string>> parameters, int delayMS, bool validateCertificate)
		{
			//https://stackoverflow.com/questions/26389899/how-do-i-disable-ssl-fallback-and-use-only-tls-for-outbound-connections-in-net/26392698#26392698
			System.Net.ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;

			if (delayMS > 0)
			{
				await Task.Delay(delayMS);
			}


			HttpWebRequest request = (HttpWebRequest)WebRequest.Create(ServerUri);

			StringBuilder postString = new StringBuilder();
			PutParam(postString, "token", "REMOVED_TOKEN");
			foreach (var p in parameters)
				PutParam(postString, p.Item1, p.Item2);

			const string contentType = "application/x-www-form-urlencoded";
			ServicePointManager.Expect100Continue = false;

			CookieContainer cookies = new CookieContainer();
			request.Method = "POST";
			request.ContentType = contentType;
			request.CookieContainer = cookies;
			request.ContentLength = postString.Length;
			request.UserAgent = "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.0.1) Gecko/2008070208 Firefox/3.0.1";
			request.Accept = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

			StreamWriter requestWriter = new StreamWriter(request.GetRequestStream());
			await requestWriter.WriteAsync(postString.ToString());
			requestWriter.Close();

			HttpWebResponse response = (HttpWebResponse)await request.GetResponseAsync();
			Stream resStream = response.GetResponseStream();

			StreamReader responseReader = new StreamReader(resStream);

			string rs = await responseReader.ReadToEndAsync();

			X509Certificate cert1 = request.ServicePoint.Certificate;
			X509Certificate2 serverCertificate = new X509Certificate2(cert1);
			if (serverCertificate.Subject != "CN=" + ServerUri.Host)
				throw new Exception("Server certificate mismatch");
			if (validateCertificate)
				await ValidateSslCertificateAsync(serverCertificate);

			return rs;
		}


		//https://stackoverflow.com/questions/6497040/how-do-i-validate-that-a-certificate-was-created-by-a-particular-certification-a
		async static Task ValidateSslCertificateAsync(X509Certificate2 certificateToValidate)
		{
			X509Chain chain = new X509Chain();
			chain.ChainPolicy.RevocationMode = X509RevocationMode.Online;
			chain.ChainPolicy.RevocationFlag = X509RevocationFlag.EntireChain;
			chain.ChainPolicy.VerificationFlags = X509VerificationFlags.NoFlag;
			chain.ChainPolicy.VerificationTime = DateTime.Now;
			chain.ChainPolicy.UrlRetrievalTimeout = new TimeSpan(0, 1, 0);

			bool isChainValid = await Task.Run<bool>(() => chain.Build(certificateToValidate));

			if (!isChainValid)
			{
				string[] errors = chain.ChainStatus
					.Select(x => String.Format("{0} ({1})", x.StatusInformation.Trim(), x.Status))
					.ToArray();
				string certificateErrorsString = "Unknown errors.";

				if (errors != null && errors.Length > 0)
				{
					certificateErrorsString = String.Join(", ", errors);
				}

				throw new Exception("Trust chain did not complete to the known authority anchor. Errors: " + certificateErrorsString);
			}

			if (chain.ChainElements.Count > 1)
				await ValidateSslCertificateAsync(chain.ChainElements[1].Certificate);
		}

		Task<string> helloTask, uploadTask;


		public IEnumerable<FileID> FetchLogFileList()
		{
			var fileList = GetFilesIn(new string[] { "client", "server", "client/ai" });

			foreach (var f in fileList)
				yield return f;

		}
		public float FontScaleFactor
		{
			get
			{
				Graphics graphics = this.CreateGraphics();
				float dpiX = graphics.DpiX;
				return dpiX / 96.0f;
			}

		}


		private int helloStarted = 0;
		private void RestartHelloTask(int delayMS = 0, string issue = "")
		{
			try
			{
				if (++helloStarted == 3)
				{
					Program.Abort(issue);
				}

				helloTask = ContactServerAsync(new Tuple<string, string>[] { new Tuple<string, string>("action", "respond") }, delayMS, true);
			}
			catch (Exception ex)
			{
				Program.Abort(ex.Message);
			}
		}

		private void ResizeFont(Control.ControlCollection coll, float scaleFactor)
		{
			foreach (Control c in coll)
			{
				if (c.HasChildren)
				{
					ResizeFont(c.Controls, scaleFactor);
				}
				c.Font = new Font(c.Font.FontFamily.Name, c.Font.Size * scaleFactor);
			}
		}

		List<FileID> logFiles = new List<FileID>();


		private void MainForm_Load(object sender, EventArgs e)
		{
			ResizeFont(this.Controls, FontScaleFactor);

			try
			{
				ProcessStartInfo startInfo = new ProcessStartInfo(Path.Combine("..", "server", "server.exe"));
				startInfo.CreateNoWindow = true;
				startInfo.UseShellExecute = false;
				startInfo.Arguments = "--version";
				startInfo.RedirectStandardOutput = true;
				startInfo.WorkingDirectory = Path.Combine("..", "server");
				Process server = Process.Start(startInfo);
				string version = server.StandardOutput.ReadToEnd();
				if (version.StartsWith("Hexaverse "))
					version = version.Substring(10);
				gameVersion.Text = version;
			}
			catch (Exception ex)
			{
				MessageBox.Show("Error trying to launch 'server --version' to figure out the current game version. You will have to specify the game version by yourself: " + ex.Message);
				//gameVersion.Text = ex.ToString();
			}

			RestartHelloTask();
			logFiles = FetchLogFileList().ToList();
			logFiles.Sort();

			FinishLoading();
		}

		

		IEnumerable<FileID> FilterLoadedFiles()
		{

			foreach (var f in logFiles)
			{
				if (string.IsNullOrWhiteSpace(f.Version) || string.IsNullOrWhiteSpace(gameVersion.Text) || f.Version.StartsWith(gameVersion.Text))
					yield return f;
			}
		}



		public void FinishLoading(List<LogFile> logFiles)
		{
			//loadedFiles = logFiles;
			FinishLoading();
		}

		private void FinishLoading()
		{
			var fileList = FilterLoadedFiles().ToList();

			var items = logFile.Items;
			items.Clear();
			items.Add("None");

			foreach (var f in fileList)
				items.Add(f);

			if (logFile.SelectedItem == null)
				logFile.SelectedIndex = 0;



			{
				string[] args = Environment.GetCommandLineArgs();
				if (args.Length > 1)
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
							if (file?.Fatal != null)
							{
								bugDescription.Text += file.Value.Fatal;
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
					MessageBox.Show("Error accessing '"+ logsAt+"': "+ex.Message);
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



		private void connectionTimer_Tick(object sender, EventArgs e)
		{
			if (uploadTask != null && helloTask == null)
			{
				if (uploadTask.IsFaulted)
				{
					Exception ex = uploadTask.Exception;
					while (ex.InnerException != null)
						ex = ex.InnerException;
					toolStripStatusLabel.Text = "Unable to upload to server: " + ex.Message.Replace("\n", "").Replace("\r", "");
					uploadTask = null;
					Program.Abort(ex.Message);
					//ActivateInput(true);
				}
				else if (uploadTask.IsCompleted)
				{
					if (uploadTask.Result == "ok")
					{
						uploadTask = null;
						MessageBox.Show("Upload succeeded. Thank you!");
						Application.Exit();
						return;
					}
					toolStripStatusLabel.Text = "Server said something unexpected: '" + uploadTask.Result + "'";
					uploadTask = null;
					ActivateInput(true);
				}
			}

			if (helloTask != null)
			{
				if (helloTask.IsFaulted)
				{
					toolStripStatusLabel.Text = "Unable to ping server: " + helloTask.Exception.InnerException.Message.Replace("\n", "").Replace("\r", "");
					RestartHelloTask(1000,helloTask.Exception.InnerException.Message);
				}
				else if (helloTask.IsCompleted)
				{
					toolStripStatusLabel.Text = helloTask.Result == "ok" ? "Server says hello" : "Server said something unexpected: '" + helloTask.Result + "'";
					helloTask = null;
					sendButton.Enabled = true;
				}
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

		private void gameVersion_TextChanged(object sender, EventArgs e)
		{
		//	if (!loadTimer.Enabled)
			FinishLoading();
		}

		private void sendButton_Click(object sender, EventArgs e)
		{
			try
			{
				if (bugDescription.Text.Trim().Length == 0)
				{
					MessageBox.Show("Please describe the problem in the top-most text field. Thank you");
					return;
				}
				ActivateInput(false);
				List<Tuple<string, string>> parameters = new List<Tuple<string, string>>();
				parameters.Add(new Tuple<string, string>("action", "store"));
				parameters.Add(new Tuple<string, string>("description", bugDescription.Text));
				parameters.Add(new Tuple<string, string>("reproduction", reproductionDesc.Text));
				parameters.Add(new Tuple<string, string>("gameVersion", gameVersion.Text));
				if (logFile.SelectedItem != null && logFile.SelectedItem is LogFile)
				{
					var log = ((LogFile)logFile.SelectedItem);
					if (log.Fatal != null)
						parameters.Add(new Tuple<string, string>("fatalMessage", log.Fatal));

					parameters.Add(new Tuple<string, string>("primaryLogFile", log + "\r\n" + log.Combined));

					foreach (var item in logFileList.SelectedItems)
					{
						LogFile f = (LogFile)item;
						parameters.Add(new Tuple<string, string>("logFile[]", f + "\r\n" + f.Combined));
					}
				}
				uploadTask = ContactServerAsync(parameters, 0, false);
			}
			catch (Exception ex)
			{
				Program.Abort(ex.Message);
			}

		}
	}
}
