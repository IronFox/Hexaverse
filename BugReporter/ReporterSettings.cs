using System;
using System.IO;
using System.Text.Json;

namespace BugReporter
{
	/// <summary>
	/// Settings read from BugReporter.json next to the executable.
	/// Nothing is compiled in: without a configured <see cref="ReportUrl"/> the reporter
	/// never contacts any server and saves reports to a local file instead.
	/// </summary>
	public sealed class ReporterSettings
	{
		public const string FileName = "BugReporter.json";

		/// <summary>HTTPS endpoint that receives reports (see docs/bug-reporter.md). Empty: save reports to a file.</summary>
		public string ReportUrl { get; set; } = "";

		/// <summary>Optional shared value sent with every request as the 'token' field.</summary>
		public string Token { get; set; } = "";

		/// <summary>Optional privacy notice shown as a link below the form. Hidden if empty.</summary>
		public string PrivacyNoticeUrl { get; set; } = "";

		/// <summary>Where users are sent to report a problem by hand.</summary>
		public string ProjectUrl { get; set; } = "https://github.com/IronFox/Hexaverse";

		/// <summary>The validated upload address, or null if reports are saved to files.</summary>
		public Uri UploadUri { get; private set; }

		/// <summary>Problem found while reading the settings, shown in the status bar.</summary>
		public string Problem { get; private set; }

		public static ReporterSettings Load()
		{
			string path = Path.Combine(AppContext.BaseDirectory, FileName);
			ReporterSettings settings = new ReporterSettings();
			if (File.Exists(path))
			{
				try
				{
					var options = new JsonSerializerOptions
					{
						PropertyNameCaseInsensitive = true,
						ReadCommentHandling = JsonCommentHandling.Skip,
						AllowTrailingCommas = true,
					};
					settings = JsonSerializer.Deserialize<ReporterSettings>(File.ReadAllText(path), options) ?? new ReporterSettings();
				}
				catch (Exception ex)
				{
					settings = new ReporterSettings();
					settings.Problem = "Unable to read " + FileName + ": " + ex.Message;
				}
			}
			settings.Validate();
			return settings;
		}

		private void Validate()
		{
			ReportUrl = (ReportUrl ?? "").Trim();
			Token = Token ?? "";
			PrivacyNoticeUrl = (PrivacyNoticeUrl ?? "").Trim();
			ProjectUrl = (ProjectUrl ?? "").Trim();

			if (ReportUrl.Length == 0)
				return;
			if (!Uri.TryCreate(ReportUrl, UriKind.Absolute, out Uri uri) || uri.Scheme != Uri.UriSchemeHttps)
			{
				Problem ??= "Ignoring reportUrl '" + ReportUrl + "': it must be an absolute https:// address";
				return;
			}
			UploadUri = uri;
		}
	}
}
