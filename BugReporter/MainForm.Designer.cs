namespace BugReporter
{
	partial class MainForm
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
			this.label1 = new System.Windows.Forms.Label();
			this.bugDescription = new System.Windows.Forms.TextBox();
			this.label2 = new System.Windows.Forms.Label();
			this.reproductionDesc = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.logFile = new System.Windows.Forms.ComboBox();
			this.label4 = new System.Windows.Forms.Label();
			this.logFileList = new System.Windows.Forms.CheckedListBox();
			this.sendButton = new System.Windows.Forms.Button();
			this.abortButton = new System.Windows.Forms.Button();
			this.label5 = new System.Windows.Forms.Label();
			this.gameVersion = new System.Windows.Forms.TextBox();
			this.statusStrip = new System.Windows.Forms.StatusStrip();
			this.toolStripStatusLabel = new System.Windows.Forms.ToolStripStatusLabel();
			this.mainBody = new System.Windows.Forms.Panel();
			this.viewPrimaryButton = new System.Windows.Forms.Button();
			this.whoAmI = new System.Windows.Forms.TextBox();
			this.label7 = new System.Windows.Forms.Label();
			this.loadTimer = new System.Windows.Forms.Timer(this.components);
			this.privacyNotice = new System.Windows.Forms.LinkLabel();
			this.statusStrip.SuspendLayout();
			this.mainBody.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 13);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(84, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "What happened?";
			// 
			// bugDescription
			// 
			this.bugDescription.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.bugDescription.Location = new System.Drawing.Point(15, 29);
			this.bugDescription.Multiline = true;
			this.bugDescription.Name = "bugDescription";
			this.bugDescription.Size = new System.Drawing.Size(474, 136);
			this.bugDescription.TabIndex = 1;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(13, 180);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(229, 13);
			this.label2.TabIndex = 2;
			this.label2.Text = "What did you do before it happened? (optional)";
			// 
			// reproductionDesc
			// 
			this.reproductionDesc.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.reproductionDesc.Location = new System.Drawing.Point(15, 197);
			this.reproductionDesc.Multiline = true;
			this.reproductionDesc.Name = "reproductionDesc";
			this.reproductionDesc.Size = new System.Drawing.Size(474, 118);
			this.reproductionDesc.TabIndex = 3;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(12, 408);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(238, 13);
			this.label3.TabIndex = 4;
			this.label3.Text = "Log file of the incident (optional but VERY useful)";
			// 
			// logFile
			// 
			this.logFile.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.logFile.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.logFile.FormattingEnabled = true;
			this.logFile.Location = new System.Drawing.Point(15, 424);
			this.logFile.Name = "logFile";
			this.logFile.Size = new System.Drawing.Size(407, 21);
			this.logFile.TabIndex = 6;
			this.logFile.SelectedIndexChanged += new System.EventHandler(this.logFile_SelectedIndexChanged);
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(12, 463);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(258, 13);
			this.label4.TabIndex = 7;
			this.label4.Text = "Related log files (check to include, right click to view)";
			// 
			// logFileList
			// 
			this.logFileList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.logFileList.FormattingEnabled = true;
			this.logFileList.Location = new System.Drawing.Point(15, 479);
			this.logFileList.Name = "logFileList";
			this.logFileList.Size = new System.Drawing.Size(474, 64);
			this.logFileList.TabIndex = 8;
			this.logFileList.MouseUp += new System.Windows.Forms.MouseEventHandler(this.logFileList_MouseUp);
			// 
			// sendButton
			// 
			this.sendButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.sendButton.Enabled = false;
			this.sendButton.Location = new System.Drawing.Point(15, 545);
			this.sendButton.Name = "sendButton";
			this.sendButton.Size = new System.Drawing.Size(235, 31);
			this.sendButton.TabIndex = 9;
			this.sendButton.Text = "Send";
			this.sendButton.UseVisualStyleBackColor = true;
			this.sendButton.Click += new System.EventHandler(this.sendButton_Click);
			// 
			// abortButton
			// 
			this.abortButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.abortButton.Location = new System.Drawing.Point(257, 545);
			this.abortButton.Name = "abortButton";
			this.abortButton.Size = new System.Drawing.Size(233, 31);
			this.abortButton.TabIndex = 10;
			this.abortButton.Text = "Cancel";
			this.abortButton.UseVisualStyleBackColor = true;
			this.abortButton.Click += new System.EventHandler(this.abortButton_Click);
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(12, 376);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(72, 13);
			this.label5.TabIndex = 12;
			this.label5.Text = "Game version";
			// 
			// gameVersion
			// 
			this.gameVersion.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.gameVersion.Location = new System.Drawing.Point(114, 373);
			this.gameVersion.Name = "gameVersion";
			this.gameVersion.Size = new System.Drawing.Size(375, 20);
			this.gameVersion.TabIndex = 13;
			// 
			// statusStrip
			// 
			this.statusStrip.ImageScalingSize = new System.Drawing.Size(20, 20);
			this.statusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripStatusLabel});
			this.statusStrip.Location = new System.Drawing.Point(0, 608);
			this.statusStrip.Name = "statusStrip";
			this.statusStrip.Size = new System.Drawing.Size(501, 22);
			this.statusStrip.TabIndex = 14;
			this.statusStrip.Text = "statusStrip1";
			// 
			// toolStripStatusLabel
			// 
			this.toolStripStatusLabel.Name = "toolStripStatusLabel";
			this.toolStripStatusLabel.Size = new System.Drawing.Size(486, 17);
			this.toolStripStatusLabel.Spring = true;
			this.toolStripStatusLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// mainBody
			// 
			this.mainBody.Controls.Add(this.privacyNotice);
			this.mainBody.Controls.Add(this.viewPrimaryButton);
			this.mainBody.Controls.Add(this.whoAmI);
			this.mainBody.Controls.Add(this.label7);
			this.mainBody.Controls.Add(this.gameVersion);
			this.mainBody.Controls.Add(this.label5);
			this.mainBody.Controls.Add(this.label1);
			this.mainBody.Controls.Add(this.label2);
			this.mainBody.Controls.Add(this.bugDescription);
			this.mainBody.Controls.Add(this.label3);
			this.mainBody.Controls.Add(this.reproductionDesc);
			this.mainBody.Controls.Add(this.label4);
			this.mainBody.Controls.Add(this.logFile);
			this.mainBody.Controls.Add(this.logFileList);
			this.mainBody.Controls.Add(this.abortButton);
			this.mainBody.Controls.Add(this.sendButton);
			this.mainBody.Dock = System.Windows.Forms.DockStyle.Fill;
			this.mainBody.Location = new System.Drawing.Point(0, 0);
			this.mainBody.Name = "mainBody";
			this.mainBody.Size = new System.Drawing.Size(501, 608);
			this.mainBody.TabIndex = 15;
			// 
			// viewPrimaryButton
			// 
			this.viewPrimaryButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.viewPrimaryButton.Location = new System.Drawing.Point(428, 423);
			this.viewPrimaryButton.Name = "viewPrimaryButton";
			this.viewPrimaryButton.Size = new System.Drawing.Size(60, 22);
			this.viewPrimaryButton.TabIndex = 16;
			this.viewPrimaryButton.Text = "View";
			this.viewPrimaryButton.UseVisualStyleBackColor = true;
			this.viewPrimaryButton.Click += new System.EventHandler(this.viewPrimaryButton_Click);
			// 
			// whoAmI
			// 
			this.whoAmI.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.whoAmI.Location = new System.Drawing.Point(168, 336);
			this.whoAmI.Name = "whoAmI";
			this.whoAmI.Size = new System.Drawing.Size(321, 20);
			this.whoAmI.TabIndex = 15;
			// 
			// label7
			// 
			this.label7.AutoSize = true;
			this.label7.Location = new System.Drawing.Point(13, 330);
			this.label7.Name = "label7";
			this.label7.Size = new System.Drawing.Size(130, 26);
			this.label7.TabIndex = 14;
			this.label7.Text = "Your email address\r\n(optional, never disclosed)";
			// 
			// loadTimer
			// 
			this.loadTimer.Enabled = true;
			this.loadTimer.Tick += new System.EventHandler(this.loadTimer_Tick);
			// 
			// privacyNotice
			// 
			this.privacyNotice.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.privacyNotice.AutoSize = true;
			this.privacyNotice.Location = new System.Drawing.Point(19, 582);
			this.privacyNotice.Name = "privacyNotice";
			this.privacyNotice.Size = new System.Drawing.Size(76, 13);
			this.privacyNotice.TabIndex = 17;
			this.privacyNotice.TabStop = true;
			this.privacyNotice.Text = "Privacy Notice";
			this.privacyNotice.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.privacyNotice_LinkClicked);
			// 
			// MainForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(501, 630);
			this.Controls.Add(this.mainBody);
			this.Controls.Add(this.statusStrip);
			this.MinimumSize = new System.Drawing.Size(513, 652);
			this.Name = "MainForm";
			this.Text = "Hexaverse Bug Reporter";
			this.Load += new System.EventHandler(this.MainForm_Load);
			this.Shown += new System.EventHandler(this.MainForm_Shown);
			this.statusStrip.ResumeLayout(false);
			this.statusStrip.PerformLayout();
			this.mainBody.ResumeLayout(false);
			this.mainBody.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox bugDescription;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox reproductionDesc;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.ComboBox logFile;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.CheckedListBox logFileList;
		private System.Windows.Forms.Button sendButton;
		private System.Windows.Forms.Button abortButton;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.TextBox gameVersion;
		private System.Windows.Forms.StatusStrip statusStrip;
		private System.Windows.Forms.ToolStripStatusLabel toolStripStatusLabel;
		private System.Windows.Forms.Panel mainBody;
		private System.Windows.Forms.Timer loadTimer;
		private System.Windows.Forms.TextBox whoAmI;
		private System.Windows.Forms.Label label7;
		private System.Windows.Forms.Button viewPrimaryButton;
		private System.Windows.Forms.LinkLabel privacyNotice;
	}
}

