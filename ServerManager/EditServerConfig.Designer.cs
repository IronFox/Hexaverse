namespace ServerManager
{
	partial class EditServerConfig
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
			this.buttonPanel = new System.Windows.Forms.Panel();
			this.abortButton = new System.Windows.Forms.Button();
			this.acceptButton = new System.Windows.Forms.Button();
			this.attributePanel = new System.Windows.Forms.Panel();
			this.toolTip = new System.Windows.Forms.ToolTip(this.components);
			this.buttonPanel.SuspendLayout();
			this.SuspendLayout();
			// 
			// buttonPanel
			// 
			this.buttonPanel.Controls.Add(this.abortButton);
			this.buttonPanel.Controls.Add(this.acceptButton);
			this.buttonPanel.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.buttonPanel.Location = new System.Drawing.Point(0, 413);
			this.buttonPanel.Name = "buttonPanel";
			this.buttonPanel.Size = new System.Drawing.Size(800, 37);
			this.buttonPanel.TabIndex = 1;
			// 
			// abortButton
			// 
			this.abortButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.abortButton.Location = new System.Drawing.Point(546, 6);
			this.abortButton.Name = "abortButton";
			this.abortButton.Size = new System.Drawing.Size(251, 23);
			this.abortButton.TabIndex = 1;
			this.abortButton.Text = "Cancel";
			this.abortButton.UseVisualStyleBackColor = true;
			this.abortButton.Click += new System.EventHandler(this.abortButton_Click);
			// 
			// acceptButton
			// 
			this.acceptButton.Location = new System.Drawing.Point(3, 6);
			this.acceptButton.Name = "acceptButton";
			this.acceptButton.Size = new System.Drawing.Size(251, 23);
			this.acceptButton.TabIndex = 0;
			this.acceptButton.Text = "button1";
			this.acceptButton.UseVisualStyleBackColor = true;
			this.acceptButton.Click += new System.EventHandler(this.acceptButton_Click);
			// 
			// attributePanel
			// 
			this.attributePanel.AutoScroll = true;
			this.attributePanel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.attributePanel.Location = new System.Drawing.Point(0, 0);
			this.attributePanel.Name = "attributePanel";
			this.attributePanel.Size = new System.Drawing.Size(800, 413);
			this.attributePanel.TabIndex = 2;
			// 
			// toolTip
			// 
			this.toolTip.AutoPopDelay = 50000;
			this.toolTip.InitialDelay = 500;
			this.toolTip.ReshowDelay = 100;
			// 
			// EditServerConfig
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(800, 450);
			this.Controls.Add(this.attributePanel);
			this.Controls.Add(this.buttonPanel);
			this.Name = "EditServerConfig";
			this.Text = "Configure Server";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.EditServerConfig_FormClosed);
			this.Load += new System.EventHandler(this.EditServerConfig_Load);
			this.buttonPanel.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion
		private System.Windows.Forms.Panel buttonPanel;
		private System.Windows.Forms.Button abortButton;
		private System.Windows.Forms.Button acceptButton;
		private System.Windows.Forms.Panel attributePanel;
		private System.Windows.Forms.ToolTip toolTip;
	}
}