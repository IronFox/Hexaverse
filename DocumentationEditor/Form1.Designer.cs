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
            this.updater = new System.Windows.Forms.Timer(this.components);
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            this.panel3.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitter1
            // 
            this.splitter1.Location = new System.Drawing.Point(603, 0);
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
            this.textEditor.Size = new System.Drawing.Size(403, 510);
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
            this.panel1.Size = new System.Drawing.Size(400, 534);
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
            this.webBrowser.Size = new System.Drawing.Size(283, 534);
            this.webBrowser.TabIndex = 0;
            // 
            // splitter2
            // 
            this.splitter2.Location = new System.Drawing.Point(200, 0);
            this.splitter2.Name = "splitter2";
            this.splitter2.Size = new System.Drawing.Size(3, 534);
            this.splitter2.TabIndex = 3;
            this.splitter2.TabStop = false;
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.webBrowser);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel2.Location = new System.Drawing.Point(606, 0);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(283, 534);
            this.panel2.TabIndex = 4;
            // 
            // panel3
            // 
            this.panel3.Controls.Add(this.documentTree);
            this.panel3.Dock = System.Windows.Forms.DockStyle.Left;
            this.panel3.Location = new System.Drawing.Point(0, 0);
            this.panel3.Name = "panel3";
            this.panel3.Size = new System.Drawing.Size(200, 534);
            this.panel3.TabIndex = 1;
            // 
            // documentTree
            // 
            this.documentTree.Dock = System.Windows.Forms.DockStyle.Fill;
            this.documentTree.Location = new System.Drawing.Point(0, 0);
            this.documentTree.Name = "documentTree";
            this.documentTree.Size = new System.Drawing.Size(200, 534);
            this.documentTree.TabIndex = 0;
            this.documentTree.MouseUp += new System.Windows.Forms.MouseEventHandler(this.documentTree_MouseUp);
            // 
            // updater
            // 
            this.updater.Enabled = true;
            this.updater.Tick += new System.EventHandler(this.updater_Tick);
            // 
            // MainWindow
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(889, 534);
            this.Controls.Add(this.panel2);
            this.Controls.Add(this.splitter1);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this.splitter2);
            this.Controls.Add(this.panel3);
            this.Name = "MainWindow";
            this.Text = "Documentation Editor";
            this.Load += new System.EventHandler(this.MainWindow_Load);
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.panel2.ResumeLayout(false);
            this.panel3.ResumeLayout(false);
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
        private System.Windows.Forms.TreeView documentTree;
        private System.Windows.Forms.Label documentName;
        private System.Windows.Forms.Timer updater;
    }
}

