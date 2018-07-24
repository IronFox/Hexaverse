namespace ServerManager
{
	partial class StartServer
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(StartServer));
			this.configureButton = new System.Windows.Forms.Button();
			this.portLabel = new System.Windows.Forms.Label();
			this.portInput = new System.Windows.Forms.TextBox();
			this.dependentCheck = new System.Windows.Forms.CheckBox();
			this.startButton = new System.Windows.Forms.Button();
			this.portStatusLabel = new System.Windows.Forms.Label();
			this.abortButton = new System.Windows.Forms.Button();
			this.portCheckTimer = new System.Windows.Forms.Timer(this.components);
			this.newGameButton = new System.Windows.Forms.RadioButton();
			this.loadSaveButton = new System.Windows.Forms.RadioButton();
			this.pickSaveGameButton = new System.Windows.Forms.Button();
			this.loadHSFDialog = new System.Windows.Forms.OpenFileDialog();
			this.saveGameLabel = new System.Windows.Forms.Label();
			this.fixPortButton = new System.Windows.Forms.Button();
			this.startOfflineCheck = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// configureButton
			// 
			this.configureButton.Location = new System.Drawing.Point(196, 87);
			this.configureButton.Name = "configureButton";
			this.configureButton.Size = new System.Drawing.Size(194, 23);
			this.configureButton.TabIndex = 0;
			this.configureButton.Text = "Configure Properties...";
			this.configureButton.UseVisualStyleBackColor = true;
			this.configureButton.Click += new System.EventHandler(this.configureButton_Click);
			// 
			// portLabel
			// 
			this.portLabel.AutoSize = true;
			this.portLabel.Location = new System.Drawing.Point(12, 9);
			this.portLabel.Name = "portLabel";
			this.portLabel.Size = new System.Drawing.Size(56, 13);
			this.portLabel.TabIndex = 1;
			this.portLabel.Text = "Base Port:";
			// 
			// portInput
			// 
			this.portInput.Location = new System.Drawing.Point(97, 6);
			this.portInput.Name = "portInput";
			this.portInput.Size = new System.Drawing.Size(159, 20);
			this.portInput.TabIndex = 2;
			this.portInput.Text = "10247";
			this.portInput.TextChanged += new System.EventHandler(this.portInput_TextChanged);
			// 
			// dependentCheck
			// 
			this.dependentCheck.AutoSize = true;
			this.dependentCheck.Checked = true;
			this.dependentCheck.CheckState = System.Windows.Forms.CheckState.Checked;
			this.dependentCheck.Location = new System.Drawing.Point(15, 32);
			this.dependentCheck.Name = "dependentCheck";
			this.dependentCheck.Size = new System.Drawing.Size(220, 17);
			this.dependentCheck.TabIndex = 3;
			this.dependentCheck.Text = "Dependent on Manager (Recommended)";
			this.dependentCheck.UseVisualStyleBackColor = true;
			// 
			// startButton
			// 
			this.startButton.Enabled = false;
			this.startButton.Location = new System.Drawing.Point(420, 145);
			this.startButton.Name = "startButton";
			this.startButton.Size = new System.Drawing.Size(104, 23);
			this.startButton.TabIndex = 4;
			this.startButton.Text = "Start Server";
			this.startButton.UseVisualStyleBackColor = true;
			this.startButton.Click += new System.EventHandler(this.startButton_Click);
			// 
			// portStatusLabel
			// 
			this.portStatusLabel.AutoSize = true;
			this.portStatusLabel.Location = new System.Drawing.Point(262, 9);
			this.portStatusLabel.Name = "portStatusLabel";
			this.portStatusLabel.Size = new System.Drawing.Size(44, 13);
			this.portStatusLabel.TabIndex = 5;
			this.portStatusLabel.Text = "Port OK";
			// 
			// abortButton
			// 
			this.abortButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.abortButton.Location = new System.Drawing.Point(542, 145);
			this.abortButton.Name = "abortButton";
			this.abortButton.Size = new System.Drawing.Size(104, 23);
			this.abortButton.TabIndex = 6;
			this.abortButton.Text = "Cancel";
			this.abortButton.UseVisualStyleBackColor = true;
			this.abortButton.Click += new System.EventHandler(this.abortButton_Click);
			// 
			// portCheckTimer
			// 
			this.portCheckTimer.Interval = 1000;
			this.portCheckTimer.Tick += new System.EventHandler(this.portCheckTimer_Tick);
			// 
			// newGameButton
			// 
			this.newGameButton.AutoSize = true;
			this.newGameButton.Checked = true;
			this.newGameButton.Location = new System.Drawing.Point(15, 90);
			this.newGameButton.Name = "newGameButton";
			this.newGameButton.Size = new System.Drawing.Size(78, 17);
			this.newGameButton.TabIndex = 7;
			this.newGameButton.TabStop = true;
			this.newGameButton.Text = "New Game";
			this.newGameButton.UseVisualStyleBackColor = true;
			this.newGameButton.CheckedChanged += new System.EventHandler(this.newGameButton_CheckedChanged);
			// 
			// loadSaveButton
			// 
			this.loadSaveButton.AutoSize = true;
			this.loadSaveButton.Location = new System.Drawing.Point(15, 113);
			this.loadSaveButton.Name = "loadSaveButton";
			this.loadSaveButton.Size = new System.Drawing.Size(108, 17);
			this.loadSaveButton.TabIndex = 8;
			this.loadSaveButton.Text = "Load Save Game";
			this.loadSaveButton.UseVisualStyleBackColor = true;
			this.loadSaveButton.CheckedChanged += new System.EventHandler(this.loadSaveButton_CheckedChanged);
			// 
			// pickSaveGameButton
			// 
			this.pickSaveGameButton.Enabled = false;
			this.pickSaveGameButton.Location = new System.Drawing.Point(196, 110);
			this.pickSaveGameButton.Name = "pickSaveGameButton";
			this.pickSaveGameButton.Size = new System.Drawing.Size(194, 23);
			this.pickSaveGameButton.TabIndex = 9;
			this.pickSaveGameButton.Text = "Choose Save Game...";
			this.pickSaveGameButton.UseVisualStyleBackColor = true;
			this.pickSaveGameButton.Click += new System.EventHandler(this.pickSaveGameButton_Click);
			// 
			// loadHSFDialog
			// 
			this.loadHSFDialog.DefaultExt = "hsf";
			this.loadHSFDialog.Filter = "Hexaverse Save File|*.hsf";
			// 
			// saveGameLabel
			// 
			this.saveGameLabel.AutoSize = true;
			this.saveGameLabel.Location = new System.Drawing.Point(396, 115);
			this.saveGameLabel.Name = "saveGameLabel";
			this.saveGameLabel.Size = new System.Drawing.Size(0, 13);
			this.saveGameLabel.TabIndex = 10;
			// 
			// fixPortButton
			// 
			this.fixPortButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.fixPortButton.Location = new System.Drawing.Point(571, 4);
			this.fixPortButton.Name = "fixPortButton";
			this.fixPortButton.Size = new System.Drawing.Size(75, 23);
			this.fixPortButton.TabIndex = 11;
			this.fixPortButton.Text = "Fix Port";
			this.fixPortButton.UseVisualStyleBackColor = true;
			this.fixPortButton.Visible = false;
			this.fixPortButton.Click += new System.EventHandler(this.fixPortButton_Click);
			// 
			// startOfflineCheck
			// 
			this.startOfflineCheck.AutoSize = true;
			this.startOfflineCheck.Location = new System.Drawing.Point(15, 55);
			this.startOfflineCheck.Name = "startOfflineCheck";
			this.startOfflineCheck.Size = new System.Drawing.Size(81, 17);
			this.startOfflineCheck.TabIndex = 12;
			this.startOfflineCheck.Text = "Start Offline";
			this.startOfflineCheck.UseVisualStyleBackColor = true;
			// 
			// StartServer
			// 
			this.AcceptButton = this.startButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.abortButton;
			this.ClientSize = new System.Drawing.Size(658, 178);
			this.Controls.Add(this.startOfflineCheck);
			this.Controls.Add(this.fixPortButton);
			this.Controls.Add(this.saveGameLabel);
			this.Controls.Add(this.pickSaveGameButton);
			this.Controls.Add(this.loadSaveButton);
			this.Controls.Add(this.newGameButton);
			this.Controls.Add(this.abortButton);
			this.Controls.Add(this.portStatusLabel);
			this.Controls.Add(this.startButton);
			this.Controls.Add(this.dependentCheck);
			this.Controls.Add(this.portInput);
			this.Controls.Add(this.portLabel);
			this.Controls.Add(this.configureButton);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.Name = "StartServer";
			this.Text = "Start Server";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.StartServer_FormClosing);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Button configureButton;
		private System.Windows.Forms.Label portLabel;
		private System.Windows.Forms.TextBox portInput;
		private System.Windows.Forms.CheckBox dependentCheck;
		private System.Windows.Forms.Button startButton;
		private System.Windows.Forms.Label portStatusLabel;
		private System.Windows.Forms.Button abortButton;
		private System.Windows.Forms.Timer portCheckTimer;
		private System.Windows.Forms.RadioButton newGameButton;
		private System.Windows.Forms.RadioButton loadSaveButton;
		private System.Windows.Forms.Button pickSaveGameButton;
		private System.Windows.Forms.OpenFileDialog loadHSFDialog;
		private System.Windows.Forms.Label saveGameLabel;
		private System.Windows.Forms.Button fixPortButton;
		private System.Windows.Forms.CheckBox startOfflineCheck;
	}
}