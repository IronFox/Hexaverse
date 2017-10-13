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
			this.label1 = new System.Windows.Forms.Label();
			this.bugDescription = new System.Windows.Forms.TextBox();
			this.label2 = new System.Windows.Forms.Label();
			this.textBox1 = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.logGroup = new System.Windows.Forms.ComboBox();
			this.logFile = new System.Windows.Forms.ComboBox();
			this.label4 = new System.Windows.Forms.Label();
			this.logFileList = new System.Windows.Forms.CheckedListBox();
			this.sendButton = new System.Windows.Forms.Button();
			this.abortButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(9, 9);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(84, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "What happend?";
			// 
			// bugDescription
			// 
			this.bugDescription.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.bugDescription.Location = new System.Drawing.Point(12, 25);
			this.bugDescription.Multiline = true;
			this.bugDescription.Name = "bugDescription";
			this.bugDescription.Size = new System.Drawing.Size(474, 169);
			this.bugDescription.TabIndex = 1;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(9, 209);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(215, 13);
			this.label2.TabIndex = 2;
			this.label2.Text = "How can we reproduce the issue? (optional)";
			// 
			// textBox1
			// 
			this.textBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.textBox1.Location = new System.Drawing.Point(12, 225);
			this.textBox1.Multiline = true;
			this.textBox1.Name = "textBox1";
			this.textBox1.Size = new System.Drawing.Size(474, 136);
			this.textBox1.TabIndex = 3;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(9, 379);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(111, 13);
			this.label3.TabIndex = 4;
			this.label3.Text = "Log file of the incident";
			// 
			// logGroup
			// 
			this.logGroup.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.logGroup.FormattingEnabled = true;
			this.logGroup.Items.AddRange(new object[] {
            "None",
            "Client",
            "Server"});
			this.logGroup.Location = new System.Drawing.Point(126, 376);
			this.logGroup.Name = "logGroup";
			this.logGroup.Size = new System.Drawing.Size(120, 21);
			this.logGroup.TabIndex = 5;
			this.logGroup.SelectedIndexChanged += new System.EventHandler(this.logGroup_SelectedIndexChanged);
			// 
			// logFile
			// 
			this.logFile.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.logFile.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.logFile.FormattingEnabled = true;
			this.logFile.Location = new System.Drawing.Point(252, 376);
			this.logFile.Name = "logFile";
			this.logFile.Size = new System.Drawing.Size(234, 21);
			this.logFile.TabIndex = 6;
			this.logFile.SelectedIndexChanged += new System.EventHandler(this.logFile_SelectedIndexChanged);
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(9, 412);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(82, 13);
			this.label4.TabIndex = 7;
			this.label4.Text = "Related log files";
			// 
			// logFileList
			// 
			this.logFileList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.logFileList.FormattingEnabled = true;
			this.logFileList.Location = new System.Drawing.Point(12, 428);
			this.logFileList.Name = "logFileList";
			this.logFileList.Size = new System.Drawing.Size(474, 124);
			this.logFileList.TabIndex = 8;
			// 
			// sendButton
			// 
			this.sendButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.sendButton.Location = new System.Drawing.Point(12, 570);
			this.sendButton.Name = "sendButton";
			this.sendButton.Size = new System.Drawing.Size(236, 31);
			this.sendButton.TabIndex = 9;
			this.sendButton.Text = "Send";
			this.sendButton.UseVisualStyleBackColor = true;
			// 
			// abortButton
			// 
			this.abortButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.abortButton.Location = new System.Drawing.Point(250, 570);
			this.abortButton.Name = "abortButton";
			this.abortButton.Size = new System.Drawing.Size(236, 31);
			this.abortButton.TabIndex = 10;
			this.abortButton.Text = "Cancel";
			this.abortButton.UseVisualStyleBackColor = true;
			// 
			// MainForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(498, 615);
			this.Controls.Add(this.abortButton);
			this.Controls.Add(this.sendButton);
			this.Controls.Add(this.logFileList);
			this.Controls.Add(this.label4);
			this.Controls.Add(this.logFile);
			this.Controls.Add(this.logGroup);
			this.Controls.Add(this.label3);
			this.Controls.Add(this.textBox1);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.bugDescription);
			this.Controls.Add(this.label1);
			this.MinimumSize = new System.Drawing.Size(514, 568);
			this.Name = "MainForm";
			this.Text = "Hexaverse Bug Reporter";
			this.Load += new System.EventHandler(this.MainForm_Load);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox bugDescription;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox textBox1;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.ComboBox logGroup;
		private System.Windows.Forms.ComboBox logFile;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.CheckedListBox logFileList;
		private System.Windows.Forms.Button sendButton;
		private System.Windows.Forms.Button abortButton;
	}
}

