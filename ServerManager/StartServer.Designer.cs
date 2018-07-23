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
			this.configureButton = new System.Windows.Forms.Button();
			this.portLabel = new System.Windows.Forms.Label();
			this.portInput = new System.Windows.Forms.TextBox();
			this.dependentCheck = new System.Windows.Forms.CheckBox();
			this.startButton = new System.Windows.Forms.Button();
			this.portStatusLabel = new System.Windows.Forms.Label();
			this.abortButton = new System.Windows.Forms.Button();
			this.portCheckTimer = new System.Windows.Forms.Timer(this.components);
			this.SuspendLayout();
			// 
			// configureButton
			// 
			this.configureButton.Location = new System.Drawing.Point(452, 4);
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
			this.dependentCheck.Size = new System.Drawing.Size(139, 17);
			this.dependentCheck.TabIndex = 3;
			this.dependentCheck.Text = "Dependent on Manager";
			this.dependentCheck.UseVisualStyleBackColor = true;
			// 
			// startButton
			// 
			this.startButton.Enabled = false;
			this.startButton.Location = new System.Drawing.Point(12, 55);
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
			this.abortButton.Location = new System.Drawing.Point(542, 55);
			this.abortButton.Name = "abortButton";
			this.abortButton.Size = new System.Drawing.Size(104, 23);
			this.abortButton.TabIndex = 6;
			this.abortButton.Text = "Cancel";
			this.abortButton.UseVisualStyleBackColor = true;
			// 
			// portCheckTimer
			// 
			this.portCheckTimer.Interval = 1000;
			this.portCheckTimer.Tick += new System.EventHandler(this.portCheckTimer_Tick);
			// 
			// StartServer
			// 
			this.AcceptButton = this.startButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.abortButton;
			this.ClientSize = new System.Drawing.Size(658, 92);
			this.Controls.Add(this.abortButton);
			this.Controls.Add(this.portStatusLabel);
			this.Controls.Add(this.startButton);
			this.Controls.Add(this.dependentCheck);
			this.Controls.Add(this.portInput);
			this.Controls.Add(this.portLabel);
			this.Controls.Add(this.configureButton);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
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
	}
}