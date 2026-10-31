using System;
using System.Windows.Forms;

namespace BugReporter
{
	public partial class Whoops : Form
	{
		public Whoops()
		{
			InitializeComponent();
		}

		string problem;

		internal void Raise(string problem)
		{
			this.problem = problem;
			theProblem.Text = problem;
			theProblem.Select(0, 0);
			this.TopMost = true;
			this.ShowDialog();
		}

		private void goAwayButton_Click(object sender, EventArgs e)
		{
			Application.Exit();
		}

		private void projectLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			string url = Program.Settings?.ProjectUrl;
			if (string.IsNullOrWhiteSpace(url))
				url = new ReporterSettings().ProjectUrl;
			try
			{
				Program.OpenExternally(url);
			}
			catch
			{ }
			Application.Exit();
		}

		private void Whoops_FormClosed(object sender, FormClosedEventArgs e)
		{
			Application.Exit();
		}
	}
}
