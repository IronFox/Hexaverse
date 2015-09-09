namespace LogDisplay
{
    partial class LogWindow
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
            this.menuStrip1 = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.menuConnect = new System.Windows.Forms.ToolStripMenuItem();
            this.menuDisconnect = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.menuQuit = new System.Windows.Forms.ToolStripMenuItem();
            this.historyToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.menuClearHistory = new System.Windows.Forms.ToolStripMenuItem();
            this.menuStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // menuStrip1
            // 
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.historyToolStripMenuItem});
            this.menuStrip1.Location = new System.Drawing.Point(0, 0);
            this.menuStrip1.Name = "menuStrip1";
            this.menuStrip1.Size = new System.Drawing.Size(880, 24);
            this.menuStrip1.TabIndex = 1;
            this.menuStrip1.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.menuConnect,
            this.menuDisconnect,
            this.toolStripSeparator1,
            this.menuQuit});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 20);
            this.fileToolStripMenuItem.Text = "File";
            // 
            // menuConnect
            // 
            this.menuConnect.Name = "menuConnect";
            this.menuConnect.Size = new System.Drawing.Size(133, 22);
            this.menuConnect.Text = "Connect";
            this.menuConnect.Click += new System.EventHandler(this.menuConnect_Click);
            // 
            // menuDisconnect
            // 
            this.menuDisconnect.Name = "menuDisconnect";
            this.menuDisconnect.Size = new System.Drawing.Size(133, 22);
            this.menuDisconnect.Text = "Disconnect";
            this.menuDisconnect.Click += new System.EventHandler(this.menuDisconnect_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(130, 6);
            // 
            // menuQuit
            // 
            this.menuQuit.Name = "menuQuit";
            this.menuQuit.Size = new System.Drawing.Size(133, 22);
            this.menuQuit.Text = "Quit";
            this.menuQuit.Click += new System.EventHandler(this.menuQuit_Click);
            // 
            // historyToolStripMenuItem
            // 
            this.historyToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.menuClearHistory});
            this.historyToolStripMenuItem.Name = "historyToolStripMenuItem";
            this.historyToolStripMenuItem.Size = new System.Drawing.Size(57, 20);
            this.historyToolStripMenuItem.Text = "History";
            // 
            // menuClearHistory
            // 
            this.menuClearHistory.Name = "menuClearHistory";
            this.menuClearHistory.Size = new System.Drawing.Size(101, 22);
            this.menuClearHistory.Text = "Clear";
            this.menuClearHistory.Click += new System.EventHandler(this.menuClearHistory_Click);
            // 
            // LogWindow
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(880, 489);
            this.Controls.Add(this.menuStrip1);
            this.MainMenuStrip = this.menuStrip1;
            this.Name = "LogWindow";
            this.Text = "Server Log";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.LogWindow_FormClosing);
            this.Load += new System.EventHandler(this.LogWindow_Load);
            this.menuStrip1.ResumeLayout(false);
            this.menuStrip1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.MenuStrip menuStrip1;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem menuConnect;
        private System.Windows.Forms.ToolStripMenuItem menuDisconnect;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem menuQuit;
        private System.Windows.Forms.ToolStripMenuItem historyToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem menuClearHistory;
    }
}

