namespace DocumentationEditor
{
	partial class MainWindow
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
            this.splitter1 = new System.Windows.Forms.Splitter();
            this.textEditor = new System.Windows.Forms.RichTextBox();
            this.panel1 = new System.Windows.Forms.Panel();
            this.documentName = new System.Windows.Forms.Label();
            this.webBrowser = new System.Windows.Forms.WebBrowser();
            this.splitter2 = new System.Windows.Forms.Splitter();
            this.panel2 = new System.Windows.Forms.Panel();
            this.panel3 = new System.Windows.Forms.Panel();
            this.documentTree = new System.Windows.Forms.TreeView();
            this.documentContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.deleteToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.updater = new System.Windows.Forms.Timer(this.components);
            this.topContentPanel = new System.Windows.Forms.Panel();
            this.logPanel = new System.Windows.Forms.Panel();
            this.logBox = new System.Windows.Forms.TextBox();
            this.editorPanel = new System.Windows.Forms.Panel();
            this.logSplitter = new System.Windows.Forms.Splitter();
            this.newFolderToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem1 = new System.Windows.Forms.ToolStripSeparator();
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            this.panel3.SuspendLayout();
            this.documentContextMenu.SuspendLayout();
            this.topContentPanel.SuspendLayout();
            this.logPanel.SuspendLayout();
            this.editorPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitter1
            // 
            this.splitter1.Location = new System.Drawing.Point(600, 0);
            this.splitter1.Name = "splitter1";
            this.splitter1.Size = new System.Drawing.Size(3, 534);
            this.splitter1.TabIndex = 0;
            this.splitter1.TabStop = false;
            // 
            // textEditor
            // 
            this.textEditor.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.textEditor.Font = new System.Drawing.Font("Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.textEditor.Location = new System.Drawing.Point(0, 24);
            this.textEditor.Name = "textEditor";
            this.textEditor.Size = new System.Drawing.Size(403, 276);
            this.textEditor.TabIndex = 1;
            this.textEditor.Text = "";
            this.textEditor.TextChanged += new System.EventHandler(this.textEditor_TextChanged);
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.documentName);
            this.panel1.Controls.Add(this.textEditor);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Left;
            this.panel1.Location = new System.Drawing.Point(203, 0);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(400, 300);
            this.panel1.TabIndex = 2;
            // 
            // documentName
            // 
            this.documentName.AutoSize = true;
            this.documentName.Location = new System.Drawing.Point(6, 8);
            this.documentName.Name = "documentName";
            this.documentName.Size = new System.Drawing.Size(82, 13);
            this.documentName.TabIndex = 2;
            this.documentName.Text = "documentName";
            // 
            // webBrowser
            // 
            this.webBrowser.Dock = System.Windows.Forms.DockStyle.Fill;
            this.webBrowser.Location = new System.Drawing.Point(0, 0);
            this.webBrowser.MinimumSize = new System.Drawing.Size(20, 20);
            this.webBrowser.Name = "webBrowser";
            this.webBrowser.Size = new System.Drawing.Size(286, 534);
            this.webBrowser.TabIndex = 0;
            this.webBrowser.Navigating += new System.Windows.Forms.WebBrowserNavigatingEventHandler(this.webBrowser_Navigating);
            // 
            // splitter2
            // 
            this.splitter2.Location = new System.Drawing.Point(200, 0);
            this.splitter2.Name = "splitter2";
            this.splitter2.Size = new System.Drawing.Size(3, 300);
            this.splitter2.TabIndex = 3;
            this.splitter2.TabStop = false;
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.webBrowser);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel2.Location = new System.Drawing.Point(603, 0);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(286, 534);
            this.panel2.TabIndex = 4;
            // 
            // panel3
            // 
            this.panel3.Controls.Add(this.documentTree);
            this.panel3.Dock = System.Windows.Forms.DockStyle.Left;
            this.panel3.Location = new System.Drawing.Point(0, 0);
            this.panel3.Name = "panel3";
            this.panel3.Size = new System.Drawing.Size(200, 300);
            this.panel3.TabIndex = 1;
            // 
            // documentTree
            // 
            this.documentTree.ContextMenuStrip = this.documentContextMenu;
            this.documentTree.Dock = System.Windows.Forms.DockStyle.Fill;
            this.documentTree.HideSelection = false;
            this.documentTree.Location = new System.Drawing.Point(0, 0);
            this.documentTree.Name = "documentTree";
            this.documentTree.Size = new System.Drawing.Size(200, 300);
            this.documentTree.TabIndex = 0;
            this.documentTree.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.documentTree_AfterSelect);
            this.documentTree.MouseUp += new System.Windows.Forms.MouseEventHandler(this.documentTree_MouseUp);
            // 
            // documentContextMenu
            // 
            this.documentContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.newFolderToolStripMenuItem,
            this.toolStripMenuItem1,
            this.deleteToolStripMenuItem});
            this.documentContextMenu.Name = "documentContextMenu";
            this.documentContextMenu.Size = new System.Drawing.Size(135, 54);
            // 
            // deleteToolStripMenuItem
            // 
            this.deleteToolStripMenuItem.Name = "deleteToolStripMenuItem";
            this.deleteToolStripMenuItem.Size = new System.Drawing.Size(152, 22);
            this.deleteToolStripMenuItem.Text = "Delete";
            this.deleteToolStripMenuItem.Click += new System.EventHandler(this.deleteToolStripMenuItem_Click);
            // 
            // updater
            // 
            this.updater.Enabled = true;
            this.updater.Tick += new System.EventHandler(this.updater_Tick);
            // 
            // topContentPanel
            // 
            this.topContentPanel.Controls.Add(this.panel1);
            this.topContentPanel.Controls.Add(this.splitter2);
            this.topContentPanel.Controls.Add(this.panel3);
            this.topContentPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.topContentPanel.Location = new System.Drawing.Point(0, 3);
            this.topContentPanel.Name = "topContentPanel";
            this.topContentPanel.Size = new System.Drawing.Size(600, 300);
            this.topContentPanel.TabIndex = 3;
            // 
            // logPanel
            // 
            this.logPanel.Controls.Add(this.logBox);
            this.logPanel.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.logPanel.Location = new System.Drawing.Point(0, 303);
            this.logPanel.Name = "logPanel";
            this.logPanel.Size = new System.Drawing.Size(600, 231);
            this.logPanel.TabIndex = 4;
            // 
            // logBox
            // 
            this.logBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.logBox.Location = new System.Drawing.Point(0, 0);
            this.logBox.Multiline = true;
            this.logBox.Name = "logBox";
            this.logBox.ReadOnly = true;
            this.logBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.logBox.Size = new System.Drawing.Size(600, 231);
            this.logBox.TabIndex = 0;
            // 
            // editorPanel
            // 
            this.editorPanel.Controls.Add(this.topContentPanel);
            this.editorPanel.Controls.Add(this.logSplitter);
            this.editorPanel.Controls.Add(this.logPanel);
            this.editorPanel.Dock = System.Windows.Forms.DockStyle.Left;
            this.editorPanel.Location = new System.Drawing.Point(0, 0);
            this.editorPanel.Name = "editorPanel";
            this.editorPanel.Size = new System.Drawing.Size(600, 534);
            this.editorPanel.TabIndex = 3;
            // 
            // logSplitter
            // 
            this.logSplitter.Cursor = System.Windows.Forms.Cursors.HSplit;
            this.logSplitter.Dock = System.Windows.Forms.DockStyle.Top;
            this.logSplitter.Location = new System.Drawing.Point(0, 0);
            this.logSplitter.Name = "logSplitter";
            this.logSplitter.Size = new System.Drawing.Size(600, 3);
            this.logSplitter.TabIndex = 0;
            this.logSplitter.TabStop = false;
            // 
            // newFolderToolStripMenuItem
            // 
            this.newFolderToolStripMenuItem.Name = "newFolderToolStripMenuItem";
            this.newFolderToolStripMenuItem.Size = new System.Drawing.Size(152, 22);
            this.newFolderToolStripMenuItem.Text = "New Folder";
            this.newFolderToolStripMenuItem.Click += new System.EventHandler(this.newFolderToolStripMenuItem_Click);
            // 
            // toolStripMenuItem1
            // 
            this.toolStripMenuItem1.Name = "toolStripMenuItem1";
            this.toolStripMenuItem1.Size = new System.Drawing.Size(131, 6);
            // 
            // MainWindow
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(889, 534);
            this.Controls.Add(this.panel2);
            this.Controls.Add(this.splitter1);
            this.Controls.Add(this.editorPanel);
            this.Name = "MainWindow";
            this.Text = "Documentation Editor";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.MainWindow_FormClosed);
            this.Load += new System.EventHandler(this.MainWindow_Load);
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.panel2.ResumeLayout(false);
            this.panel3.ResumeLayout(false);
            this.documentContextMenu.ResumeLayout(false);
            this.topContentPanel.ResumeLayout(false);
            this.logPanel.ResumeLayout(false);
            this.logPanel.PerformLayout();
            this.editorPanel.ResumeLayout(false);
            this.ResumeLayout(false);

		}

        #endregion

        private System.Windows.Forms.Splitter splitter1;
        private System.Windows.Forms.RichTextBox textEditor;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.WebBrowser webBrowser;
        private System.Windows.Forms.Splitter splitter2;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.Panel panel3;
        private System.Windows.Forms.Label documentName;
        private System.Windows.Forms.Timer updater;
        private System.Windows.Forms.Panel topContentPanel;
        private System.Windows.Forms.Panel logPanel;
        private System.Windows.Forms.TextBox logBox;
        private System.Windows.Forms.Panel editorPanel;
        private System.Windows.Forms.Splitter logSplitter;
        private System.Windows.Forms.ContextMenuStrip documentContextMenu;
        private System.Windows.Forms.ToolStripMenuItem deleteToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem newFolderToolStripMenuItem;
        internal System.Windows.Forms.TreeView documentTree;
        private System.Windows.Forms.ToolStripSeparator toolStripMenuItem1;
    }
}

