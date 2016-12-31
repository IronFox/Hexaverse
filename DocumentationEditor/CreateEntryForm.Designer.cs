namespace DocumentationEditor
{
    partial class CreateEntryForm
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
            this.scope = new System.Windows.Forms.Label();
            this.resultInput = new System.Windows.Forms.TextBox();
            this.resultReport = new System.Windows.Forms.Label();
            this.createButton = new System.Windows.Forms.Button();
            this.abortButton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // scope
            // 
            this.scope.AutoSize = true;
            this.scope.Location = new System.Drawing.Point(12, 9);
            this.scope.Name = "scope";
            this.scope.Size = new System.Drawing.Size(35, 13);
            this.scope.TabIndex = 0;
            this.scope.Text = "label1";
            // 
            // resultInput
            // 
            this.resultInput.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.resultInput.Location = new System.Drawing.Point(12, 25);
            this.resultInput.Name = "resultInput";
            this.resultInput.Size = new System.Drawing.Size(260, 20);
            this.resultInput.TabIndex = 1;
            this.resultInput.TextChanged += new System.EventHandler(this.resultInput_TextChanged);
            // 
            // resultReport
            // 
            this.resultReport.AutoSize = true;
            this.resultReport.Location = new System.Drawing.Point(12, 48);
            this.resultReport.Name = "resultReport";
            this.resultReport.Size = new System.Drawing.Size(35, 13);
            this.resultReport.TabIndex = 2;
            this.resultReport.Text = "label1";
            // 
            // createButton
            // 
            this.createButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.createButton.Location = new System.Drawing.Point(116, 64);
            this.createButton.Name = "createButton";
            this.createButton.Size = new System.Drawing.Size(75, 23);
            this.createButton.TabIndex = 3;
            this.createButton.Text = "Create";
            this.createButton.UseVisualStyleBackColor = true;
            this.createButton.Click += new System.EventHandler(this.createButton_Click);
            // 
            // abortButton
            // 
            this.abortButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.abortButton.Location = new System.Drawing.Point(197, 64);
            this.abortButton.Name = "abortButton";
            this.abortButton.Size = new System.Drawing.Size(75, 23);
            this.abortButton.TabIndex = 4;
            this.abortButton.Text = "Cancel";
            this.abortButton.UseVisualStyleBackColor = true;
            this.abortButton.Click += new System.EventHandler(this.abortButton_Click);
            // 
            // CreateEntryForm
            // 
            this.AcceptButton = this.createButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.abortButton;
            this.ClientSize = new System.Drawing.Size(284, 96);
            this.Controls.Add(this.abortButton);
            this.Controls.Add(this.createButton);
            this.Controls.Add(this.resultReport);
            this.Controls.Add(this.resultInput);
            this.Controls.Add(this.scope);
            this.MaximumSize = new System.Drawing.Size(800, 134);
            this.MinimumSize = new System.Drawing.Size(300, 134);
            this.Name = "CreateEntryForm";
            this.Text = "CreateEntryForm";
            this.Activated += new System.EventHandler(this.CreateEntryForm_Activated);
            this.Shown += new System.EventHandler(this.CreateEntryForm_Shown);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label scope;
        private System.Windows.Forms.TextBox resultInput;
        private System.Windows.Forms.Label resultReport;
        private System.Windows.Forms.Button createButton;
        private System.Windows.Forms.Button abortButton;
    }
}