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
			this.panel2 = new System.Windows.Forms.Panel();
			this.acceptButton = new System.Windows.Forms.Button();
			this.abortButton = new System.Windows.Forms.Button();
			this.attributePanel = new System.Windows.Forms.Panel();
			this.toolTip = new System.Windows.Forms.ToolTip(this.components);
			this.panel2.SuspendLayout();
			this.SuspendLayout();
			// 
			// panel2
			// 
			this.panel2.Controls.Add(this.abortButton);
			this.panel2.Controls.Add(this.acceptButton);
			this.panel2.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.panel2.Location = new System.Drawing.Point(0, 413);
			this.panel2.Name = "panel2";
			this.panel2.Size = new System.Drawing.Size(800, 37);
			this.panel2.TabIndex = 1;
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
			// attributePanel
			// 
			this.attributePanel.AutoScroll = true;
			this.attributePanel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.attributePanel.Location = new System.Drawing.Point(0, 0);
			this.attributePanel.Name = "attributePanel";
			this.attributePanel.Size = new System.Drawing.Size(800, 413);
			this.attributePanel.TabIndex = 2;
			// 
			// EditServerConfig
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(800, 450);
			this.Controls.Add(this.attributePanel);
			this.Controls.Add(this.panel2);
			this.Name = "EditServerConfig";
			this.Text = "Configure Server";
			this.panel2.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion
		private System.Windows.Forms.Panel panel2;
		private System.Windows.Forms.Button abortButton;
		private System.Windows.Forms.Button acceptButton;
		private System.Windows.Forms.Panel attributePanel;
		private System.Windows.Forms.ToolTip toolTip;
	}
}