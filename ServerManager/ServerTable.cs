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

namespace ServerManager
{
	public partial class ServerTable : Form
	{
		public ServerTable()
		{
			InitializeComponent();
		}

		private void visibilityStripItem_Click(object sender, EventArgs e)
		{

		}


		private List<Action<string>> attributeListHandler = new List<Action<string>>();

		private void FlushAttributes()
		{
			var attribs = globalAttributeView.Items;
			attribs.Clear();
			attributeListHandler.Clear();
		}

		private void AddAttribute(string name, string value, string tooltip, Action<string> onClickHandleValue)
		{
			var row = globalAttributeView.Items.Add("");
			row.SubItems.Add(name+":");
			row.SubItems.Add(value);
			row.ToolTipText = tooltip;
			if (onClickHandleValue != null)
				row.ToolTipText += ". Double click to open";
			attributeListHandler.Add(onClickHandleValue);

			globalAttributeView.ShowItemToolTips = true;

			attributeNameHeader.Width = -1;
			attributeValueHeader.Width = -1;
		}



		private void ServerTable_Load(object sender, EventArgs e)
		{
			FlushAttributes();
			AddAttribute("Version", Program.gameVersion, "Local Hexaverse version", null);
			AddAttribute("Install Dir", Directory.GetCurrentDirectory(), "Hexaverse Installation Directory", (dir) =>
			  {
				  Process.Start("explorer.exe", dir);
			  });
			{
				string myDocs = System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
				string gameDir = Path.Combine(myDocs, "My Games", "Hexaverse");
				Directory.CreateDirectory(gameDir);
				AddAttribute("Game Dir", gameDir, "Hexaverse Game Directory", (dir) =>
				   {
					   Process.Start("explorer.exe", dir);
				   });
				string cfg = Path.Combine(gameDir, "server", "config", "server.ini");
				AddAttribute("Server Cfg", cfg, "Server Configuration File", (path) =>
				   {
					   OpenServerConfiguration("Save Configuration", (path2) =>
					   {
						   File.Delete(path);
						   File.Move(path2, path);
					   });
				   });
			}


			//this.globalAttributeView
		}

		private void OpenServerConfiguration(string commitText, Action<string> onCompleteHandlePath)
		{
			new EditServerConfig().Show(commitText, onCompleteHandlePath);
		}

		private void globalAttributeView_DoubleClick(object sender, EventArgs e)
		{
			if (globalAttributeView.SelectedIndices.Count == 1)
			{
				var at = globalAttributeView.SelectedIndices[0];
				if (at < attributeListHandler.Count && attributeListHandler[at] != null)
					attributeListHandler[at](globalAttributeView.Items[at].SubItems[2].Text);
			}
		}
	}

}
