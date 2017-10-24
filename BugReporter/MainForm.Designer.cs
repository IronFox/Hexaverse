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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
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
			this.connectionTimer = new System.Windows.Forms.Timer(this.components);
			this.mainBody = new System.Windows.Forms.Panel();
			this.statusStrip.SuspendLayout();
			this.mainBody.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(16, 16);
			this.label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(109, 17);
			this.label1.TabIndex = 0;
			this.label1.Text = "What happend?";
			// 
			// bugDescription
			// 
			this.bugDescription.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.bugDescription.Location = new System.Drawing.Point(20, 36);
			this.bugDescription.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.bugDescription.Multiline = true;
			this.bugDescription.Name = "bugDescription";
			this.bugDescription.Size = new System.Drawing.Size(631, 166);
			this.bugDescription.TabIndex = 1;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(16, 223);
			this.label2.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(285, 17);
			this.label2.TabIndex = 2;
			this.label2.Text = "How can we reproduce the issue? (optional)";
			// 
			// reproductionDesc
			// 
			this.reproductionDesc.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.reproductionDesc.Location = new System.Drawing.Point(20, 242);
			this.reproductionDesc.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.reproductionDesc.Multiline = true;
			this.reproductionDesc.Name = "reproductionDesc";
			this.reproductionDesc.Size = new System.Drawing.Size(631, 166);
			this.reproductionDesc.TabIndex = 3;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(16, 466);
			this.label3.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(318, 17);
			this.label3.TabIndex = 4;
			this.label3.Text = "Log file of the incident (optional but VERY useful)";
			// 
			// logFile
			// 
			this.logFile.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.logFile.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.logFile.FormattingEnabled = true;
			this.logFile.Location = new System.Drawing.Point(20, 486);
			this.logFile.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.logFile.Name = "logFile";
			this.logFile.Size = new System.Drawing.Size(631, 24);
			this.logFile.TabIndex = 6;
			this.logFile.SelectedIndexChanged += new System.EventHandler(this.logFile_SelectedIndexChanged);
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(16, 534);
			this.label4.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(225, 17);
			this.label4.TabIndex = 7;
			this.label4.Text = "Related log files (check to include)";
			// 
			// logFileList
			// 
			this.logFileList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.logFileList.FormattingEnabled = true;
			this.logFileList.Location = new System.Drawing.Point(20, 554);
			this.logFileList.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.logFileList.Name = "logFileList";
			this.logFileList.Size = new System.Drawing.Size(631, 140);
			this.logFileList.TabIndex = 8;
			// 
			// sendButton
			// 
			this.sendButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.sendButton.Enabled = false;
			this.sendButton.Location = new System.Drawing.Point(20, 723);
			this.sendButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.sendButton.Name = "sendButton";
			this.sendButton.Size = new System.Drawing.Size(313, 38);
			this.sendButton.TabIndex = 9;
			this.sendButton.Text = "Send";
			this.sendButton.UseVisualStyleBackColor = true;
			this.sendButton.Click += new System.EventHandler(this.sendButton_Click);
			// 
			// abortButton
			// 
			this.abortButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.abortButton.Location = new System.Drawing.Point(341, 723);
			this.abortButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.abortButton.Name = "abortButton";
			this.abortButton.Size = new System.Drawing.Size(311, 38);
			this.abortButton.TabIndex = 10;
			this.abortButton.Text = "Cancel";
			this.abortButton.UseVisualStyleBackColor = true;
			this.abortButton.Click += new System.EventHandler(this.abortButton_Click);
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(16, 427);
			this.label5.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(96, 17);
			this.label5.TabIndex = 12;
			this.label5.Text = "Game version";
			// 
			// gameVersion
			// 
			this.gameVersion.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.gameVersion.Location = new System.Drawing.Point(120, 423);
			this.gameVersion.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.gameVersion.Name = "gameVersion";
			this.gameVersion.Size = new System.Drawing.Size(531, 22);
			this.gameVersion.TabIndex = 13;
			// 
			// statusStrip
			// 
			this.statusStrip.ImageScalingSize = new System.Drawing.Size(20, 20);
			this.statusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripStatusLabel});
			this.statusStrip.Location = new System.Drawing.Point(0, 777);
			this.statusStrip.Name = "statusStrip";
			this.statusStrip.Padding = new System.Windows.Forms.Padding(1, 0, 19, 0);
			this.statusStrip.Size = new System.Drawing.Size(668, 22);
			this.statusStrip.TabIndex = 14;
			this.statusStrip.Text = "statusStrip1";
			// 
			// toolStripStatusLabel
			// 
			this.toolStripStatusLabel.Name = "toolStripStatusLabel";
			this.toolStripStatusLabel.Size = new System.Drawing.Size(648, 17);
			this.toolStripStatusLabel.Spring = true;
			this.toolStripStatusLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// connectionTimer
			// 
			this.connectionTimer.Enabled = true;
			this.connectionTimer.Tick += new System.EventHandler(this.connectionTimer_Tick);
			// 
			// mainBody
			// 
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
			this.mainBody.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.mainBody.Name = "mainBody";
			this.mainBody.Size = new System.Drawing.Size(668, 777);
			this.mainBody.TabIndex = 15;
			// 
			// MainForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(668, 799);
			this.Controls.Add(this.mainBody);
			this.Controls.Add(this.statusStrip);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			this.MinimumSize = new System.Drawing.Size(679, 794);
			this.Name = "MainForm";
			this.Text = "Hexaverse Bug Reporter";
			this.Load += new System.EventHandler(this.MainForm_Load);
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
		private System.Windows.Forms.Timer connectionTimer;
		private System.Windows.Forms.ToolStripStatusLabel toolStripStatusLabel;
		private System.Windows.Forms.Panel mainBody;
	}
}

