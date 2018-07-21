namespace ServerManager
{
	partial class ServerTable
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.Windows.Forms.ListViewItem listViewItem1 = new System.Windows.Forms.ListViewItem(new string[] {
            "",
            "Game Version",
            "v?.?.?.?"}, -1);
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ServerTable));
			this.globalInfoPanel = new System.Windows.Forms.Panel();
			this.launchServerButton = new System.Windows.Forms.Button();
			this.globalAttributeView = new System.Windows.Forms.ListView();
			this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.attributeNameHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.attributeValueHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.launchClientButton = new System.Windows.Forms.Button();
			this.serverListView = new System.Windows.Forms.ListView();
			this.hostIDHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.worldIDHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.portHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.statusHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.ruleHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.statusStrip1 = new System.Windows.Forms.StatusStrip();
			this.menuStrip1 = new System.Windows.Forms.MenuStrip();
			this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.serverContextMenuStrip = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.controlStripItem = new System.Windows.Forms.ToolStripMenuItem();
			this.logStripItem = new System.Windows.Forms.ToolStripMenuItem();
			this.saveGameAsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.visibilityStripItem = new System.Windows.Forms.ToolStripComboBox();
			this.passwordStripItem = new System.Windows.Forms.ToolStripTextBox();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.shutdownStripItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
			this.receiverTimer = new System.Windows.Forms.Timer(this.components);
			this.globalInfoPanel.SuspendLayout();
			this.menuStrip1.SuspendLayout();
			this.serverContextMenuStrip.SuspendLayout();
			this.SuspendLayout();
			// 
			// globalInfoPanel
			// 
			this.globalInfoPanel.Controls.Add(this.launchServerButton);
			this.globalInfoPanel.Controls.Add(this.globalAttributeView);
			this.globalInfoPanel.Controls.Add(this.launchClientButton);
			this.globalInfoPanel.Dock = System.Windows.Forms.DockStyle.Top;
			this.globalInfoPanel.Location = new System.Drawing.Point(0, 24);
			this.globalInfoPanel.Name = "globalInfoPanel";
			this.globalInfoPanel.Size = new System.Drawing.Size(800, 117);
			this.globalInfoPanel.TabIndex = 0;
			// 
			// launchServerButton
			// 
			this.launchServerButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.launchServerButton.Image = global::ServerManager.Properties.Resources.hexaverse_server_capsule_87;
			this.launchServerButton.ImageAlign = System.Drawing.ContentAlignment.TopCenter;
			this.launchServerButton.Location = new System.Drawing.Point(550, 3);
			this.launchServerButton.Name = "launchServerButton";
			this.launchServerButton.Size = new System.Drawing.Size(247, 110);
			this.launchServerButton.TabIndex = 8;
			this.launchServerButton.Text = "Add Server...";
			this.launchServerButton.TextAlign = System.Drawing.ContentAlignment.BottomCenter;
			this.launchServerButton.UseVisualStyleBackColor = true;
			// 
			// globalAttributeView
			// 
			this.globalAttributeView.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.globalAttributeView.BackColor = System.Drawing.SystemColors.Control;
			this.globalAttributeView.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.globalAttributeView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1,
            this.attributeNameHeader,
            this.attributeValueHeader});
			this.globalAttributeView.FullRowSelect = true;
			this.globalAttributeView.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.None;
			this.globalAttributeView.Items.AddRange(new System.Windows.Forms.ListViewItem[] {
            listViewItem1});
			this.globalAttributeView.Location = new System.Drawing.Point(256, 3);
			this.globalAttributeView.MultiSelect = false;
			this.globalAttributeView.Name = "globalAttributeView";
			this.globalAttributeView.ShowItemToolTips = true;
			this.globalAttributeView.Size = new System.Drawing.Size(288, 110);
			this.globalAttributeView.TabIndex = 7;
			this.globalAttributeView.UseCompatibleStateImageBehavior = false;
			this.globalAttributeView.View = System.Windows.Forms.View.Details;
			this.globalAttributeView.DoubleClick += new System.EventHandler(this.globalAttributeView_DoubleClick);
			// 
			// columnHeader1
			// 
			this.columnHeader1.Text = "";
			this.columnHeader1.Width = 1;
			// 
			// attributeNameHeader
			// 
			this.attributeNameHeader.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
			this.attributeNameHeader.Width = 82;
			// 
			// attributeValueHeader
			// 
			this.attributeValueHeader.Width = 300;
			// 
			// launchClientButton
			// 
			this.launchClientButton.Image = global::ServerManager.Properties.Resources.hexaverse_cover_capsule_87;
			this.launchClientButton.ImageAlign = System.Drawing.ContentAlignment.TopCenter;
			this.launchClientButton.Location = new System.Drawing.Point(3, 3);
			this.launchClientButton.Name = "launchClientButton";
			this.launchClientButton.Size = new System.Drawing.Size(247, 110);
			this.launchClientButton.TabIndex = 0;
			this.launchClientButton.Text = "Launch Client";
			this.launchClientButton.TextAlign = System.Drawing.ContentAlignment.BottomCenter;
			this.launchClientButton.UseVisualStyleBackColor = true;
			// 
			// serverListView
			// 
			this.serverListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.portHeader,
            this.hostIDHeader,
            this.worldIDHeader,
            this.statusHeader,
            this.ruleHeader});
			this.serverListView.Dock = System.Windows.Forms.DockStyle.Fill;
			this.serverListView.FullRowSelect = true;
			this.serverListView.Location = new System.Drawing.Point(0, 141);
			this.serverListView.Name = "serverListView";
			this.serverListView.Size = new System.Drawing.Size(800, 287);
			this.serverListView.TabIndex = 1;
			this.serverListView.UseCompatibleStateImageBehavior = false;
			this.serverListView.View = System.Windows.Forms.View.Details;
			// 
			// hostIDHeader
			// 
			this.hostIDHeader.Text = "Server ID";
			this.hostIDHeader.Width = 100;
			// 
			// worldIDHeader
			// 
			this.worldIDHeader.Text = "World ID";
			this.worldIDHeader.Width = 100;
			// 
			// portHeader
			// 
			this.portHeader.Text = "Port Range";
			this.portHeader.Width = 100;
			// 
			// statusHeader
			// 
			this.statusHeader.Text = "Status";
			this.statusHeader.Width = 200;
			// 
			// ruleHeader
			// 
			this.ruleHeader.Text = "Rules";
			this.ruleHeader.Width = 200;
			// 
			// statusStrip1
			// 
			this.statusStrip1.Location = new System.Drawing.Point(0, 428);
			this.statusStrip1.Name = "statusStrip1";
			this.statusStrip1.Size = new System.Drawing.Size(800, 22);
			this.statusStrip1.TabIndex = 2;
			this.statusStrip1.Text = "statusStrip1";
			// 
			// menuStrip1
			// 
			this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem});
			this.menuStrip1.Location = new System.Drawing.Point(0, 0);
			this.menuStrip1.Name = "menuStrip1";
			this.menuStrip1.Size = new System.Drawing.Size(800, 24);
			this.menuStrip1.TabIndex = 3;
			this.menuStrip1.Text = "menuStrip1";
			// 
			// fileToolStripMenuItem
			// 
			this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.exitToolStripMenuItem});
			this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
			this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 20);
			this.fileToolStripMenuItem.Text = "File";
			// 
			// exitToolStripMenuItem
			// 
			this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
			this.exitToolStripMenuItem.Size = new System.Drawing.Size(92, 22);
			this.exitToolStripMenuItem.Text = "Exit";
			// 
			// serverContextMenuStrip
			// 
			this.serverContextMenuStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.controlStripItem,
            this.logStripItem,
            this.saveGameAsToolStripMenuItem,
            this.visibilityStripItem,
            this.passwordStripItem,
            this.toolStripSeparator1,
            this.shutdownStripItem,
            this.toolStripSeparator2});
			this.serverContextMenuStrip.Name = "serverContextMenuStrip";
			this.serverContextMenuStrip.Size = new System.Drawing.Size(209, 156);
			// 
			// controlStripItem
			// 
			this.controlStripItem.Name = "controlStripItem";
			this.controlStripItem.Size = new System.Drawing.Size(208, 22);
			this.controlStripItem.Text = "Connect Control Console";
			// 
			// logStripItem
			// 
			this.logStripItem.Name = "logStripItem";
			this.logStripItem.Size = new System.Drawing.Size(208, 22);
			this.logStripItem.Text = "View Log File";
			// 
			// saveGameAsToolStripMenuItem
			// 
			this.saveGameAsToolStripMenuItem.Name = "saveGameAsToolStripMenuItem";
			this.saveGameAsToolStripMenuItem.Size = new System.Drawing.Size(208, 22);
			this.saveGameAsToolStripMenuItem.Text = "Save Game as...";
			// 
			// visibilityStripItem
			// 
			this.visibilityStripItem.Items.AddRange(new object[] {
            "Private",
            "Friends Only",
            "Public"});
			this.visibilityStripItem.Name = "visibilityStripItem";
			this.visibilityStripItem.Size = new System.Drawing.Size(121, 23);
			this.visibilityStripItem.Text = "Visibility:";
			this.visibilityStripItem.Click += new System.EventHandler(this.visibilityStripItem_Click);
			// 
			// passwordStripItem
			// 
			this.passwordStripItem.Name = "passwordStripItem";
			this.passwordStripItem.Size = new System.Drawing.Size(100, 23);
			this.passwordStripItem.Text = "Password:";
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size(205, 6);
			// 
			// shutdownStripItem
			// 
			this.shutdownStripItem.Name = "shutdownStripItem";
			this.shutdownStripItem.Size = new System.Drawing.Size(208, 22);
			this.shutdownStripItem.Text = "Shut Down";
			// 
			// toolStripSeparator2
			// 
			this.toolStripSeparator2.Name = "toolStripSeparator2";
			this.toolStripSeparator2.Size = new System.Drawing.Size(205, 6);
			// 
			// receiverTimer
			// 
			this.receiverTimer.Interval = 1000;
			this.receiverTimer.Tick += new System.EventHandler(this.receiverTimer_Tick);
			// 
			// ServerTable
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(800, 450);
			this.Controls.Add(this.serverListView);
			this.Controls.Add(this.statusStrip1);
			this.Controls.Add(this.globalInfoPanel);
			this.Controls.Add(this.menuStrip1);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MainMenuStrip = this.menuStrip1;
			this.MinimumSize = new System.Drawing.Size(816, 489);
			this.Name = "ServerTable";
			this.Text = "Server Manager";
			this.Load += new System.EventHandler(this.ServerTable_Load);
			this.globalInfoPanel.ResumeLayout(false);
			this.menuStrip1.ResumeLayout(false);
			this.menuStrip1.PerformLayout();
			this.serverContextMenuStrip.ResumeLayout(false);
			this.serverContextMenuStrip.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Panel globalInfoPanel;
		private System.Windows.Forms.Button launchClientButton;
		private System.Windows.Forms.ListView globalAttributeView;
		private System.Windows.Forms.ColumnHeader columnHeader1;
		private System.Windows.Forms.ColumnHeader attributeValueHeader;
		private System.Windows.Forms.ListView serverListView;
		private System.Windows.Forms.ColumnHeader hostIDHeader;
		private System.Windows.Forms.ColumnHeader worldIDHeader;
		private System.Windows.Forms.ColumnHeader portHeader;
		private System.Windows.Forms.ColumnHeader statusHeader;
		private System.Windows.Forms.ColumnHeader ruleHeader;
		private System.Windows.Forms.StatusStrip statusStrip1;
		private System.Windows.Forms.MenuStrip menuStrip1;
		private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
		private System.Windows.Forms.ContextMenuStrip serverContextMenuStrip;
		private System.Windows.Forms.ToolStripMenuItem controlStripItem;
		private System.Windows.Forms.ToolStripMenuItem logStripItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripMenuItem shutdownStripItem;
		private System.Windows.Forms.ToolStripComboBox visibilityStripItem;
		private System.Windows.Forms.Button launchServerButton;
		private System.Windows.Forms.ToolStripMenuItem saveGameAsToolStripMenuItem;
		private System.Windows.Forms.ToolStripTextBox passwordStripItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.ColumnHeader attributeNameHeader;
		private System.Windows.Forms.Timer receiverTimer;
	}
}

