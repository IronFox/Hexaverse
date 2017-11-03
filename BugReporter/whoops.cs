using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
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
			this.ShowDialog();
		}

		private void goAwayButton_Click(object sender, EventArgs e)
		{
			Application.Exit();
		}

		private void emailLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			string path = "mailto:bugs@hexaverse.net?subject=Bug-Reporter won't work&body="+ problem;

			System.Diagnostics.Process.Start(path);
			Application.Exit();
		}

		private void Whoops_FormClosed(object sender, FormClosedEventArgs e)
		{
			Application.Exit();
		}
	}
}
