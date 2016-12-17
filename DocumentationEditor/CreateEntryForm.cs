using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DocumentationEditor
{
    public partial class CreateEntryForm : Form
    {
        public CreateEntryForm()
        {
            InitializeComponent();
        }

        FolderInfo parent;
        bool createDocument;
        MainWindow caller;

        public void Show(MainWindow caller, FolderInfo parent, bool createDocument)
        {
            this.parent = parent;
            this.createDocument = createDocument;
            this.caller = caller;

            this.Text = "Create new " + (createDocument ? "Document" : "Folder");

            scope.Text = "Create in '" + parent.NiceName+"'";
            CheckInput();


            this.Show();
        }

        private string GetText()
        {
            return resultInput.Text.Trim();
        }

        private void CheckInput()
        {
            string test = GetText();
            try
            {
                if (parent.Removed)
                {
                    Close();
                    return;
                }
                if (createDocument)
                {
                    File.VerifyIsValidName(test);
                    if (parent.FindDocument(test) != null)
                        throw new Exception("Document exists");
                }
                else
                {
                    Directory.VerifyIsValidName(test);
                    if (parent.FindFolder(test) != null)
                        throw new Exception("Folder exists");
                }
                resultReport.Text = "ok";
                createButton.Enabled = true;
            }
            catch (Exception ex)
            {
                resultReport.Text = ex.Message;
                createButton.Enabled = false;
            }

        }


        private void CreateEntryForm_Shown(object sender, EventArgs e)
        {
            
        }

        private void CreateEntryForm_Activated(object sender, EventArgs e)
        {
            CheckInput();
        }

        private void abortButton_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void createButton_Click(object sender, EventArgs e)
        {
            CheckInput();
            if (createButton.Enabled)
            {
                try
                {
                    string text = GetText();
                    if (createDocument)
                    {
                        File file = new File(Path.Combine(parent.Directory.FullName, text + ".txt"));
                        file.Text = ""; //create file
                        Document doc = caller.RegisterFile(caller.DocumentRoot.Directory, parent, file);
                        caller.documentTree.SelectedNode = doc.Node;
                    }
                    else
                    {
                        Directory dir = parent.Directory.CreateSubdirectory(text);

                        parent.RegisterChild(dir);
                    }
                    Close();
                }
                catch (Exception ex)
                {
                    caller.Log(ex);
                }
            }
        }

        private void resultInput_TextChanged(object sender, EventArgs e)
        {
            CheckInput();
        }


    }
}
