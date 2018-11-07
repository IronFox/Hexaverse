namespace BugReporter
{
	partial class Whoops
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
			this.theProblem = new System.Windows.Forms.TextBox();
			this.emailLink = new System.Windows.Forms.LinkLabel();
			this.label1 = new System.Windows.Forms.Label();
			this.goAwayButton = new System.Windows.Forms.Button();
			this.label2 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// theProblem
			// 
			this.theProblem.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.theProblem.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.theProblem.Location = new System.Drawing.Point(12, 64);
			this.theProblem.Multiline = true;
			this.theProblem.Name = "theProblem";
			this.theProblem.ReadOnly = true;
			this.theProblem.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
			this.theProblem.Size = new System.Drawing.Size(450, 207);
			this.theProblem.TabIndex = 0;
			this.theProblem.Text = "Now this is embarassing.\r\nSomething went wrong, and it seems reports cannot be up" +
    "loaded with now:";
			// 
			// emailLink
			// 
			this.emailLink.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.emailLink.AutoSize = true;
			this.emailLink.Location = new System.Drawing.Point(9, 291);
			this.emailLink.Name = "emailLink";
			this.emailLink.Size = new System.Drawing.Size(277, 17);
			this.emailLink.TabIndex = 1;
			this.emailLink.TabStop = true;
			this.emailLink.Text = "Open E-Mail client to bugs@hexaverse.net";
			this.emailLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.emailLink_LinkClicked);
			// 
			// label1
			// 
			this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(9, 274);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(336, 17);
			this.label1.TabIndex = 2;
			this.label1.Text = "Would you like to try and send an E-Mail about this?";
			// 
			// goAwayButton
			// 
			this.goAwayButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.goAwayButton.Location = new System.Drawing.Point(282, 343);
			this.goAwayButton.Name = "goAwayButton";
			this.goAwayButton.Size = new System.Drawing.Size(180, 32);
			this.goAwayButton.TabIndex = 3;
			this.goAwayButton.Text = "No! Go Away!";
			this.goAwayButton.UseVisualStyleBackColor = true;
			this.goAwayButton.Click += new System.EventHandler(this.goAwayButton_Click);
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.label2.Location = new System.Drawing.Point(8, 12);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(201, 20);
			this.label2.TabIndex = 4;
			this.label2.Text = "Well, this is embarassing.";
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.label3.Location = new System.Drawing.Point(8, 32);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(308, 20);
			this.label3.TabIndex = 5;
			this.label3.Text = "Something appears to have gone wrong:";
			// 
			// Whoops
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(474, 387);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.label3);
			this.Controls.Add(this.goAwayButton);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.emailLink);
			this.Controls.Add(this.theProblem);
			this.MinimumSize = new System.Drawing.Size(492, 434);
			this.Name = "Whoops";
			this.Text = "Whoops";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.Whoops_FormClosed);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox theProblem;
		private System.Windows.Forms.LinkLabel emailLink;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button goAwayButton;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Label label3;
	}
}