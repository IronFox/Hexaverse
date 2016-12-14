using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DocumentationEditor
{


    public struct File
    {
        string fullPath;

        public File(string path)
        {
            this.fullPath = path != null ? new System.IO.FileInfo(path).FullName : null;
        }

        public string FullName
        {
            get
            {
                return fullPath;
            }
        }

        public bool Exists
        {
            get
            {
                return fullPath != null && System.IO.File.Exists(fullPath);
            }
        }

        public bool IsEmpty
        {
            get
            {
                return fullPath == null;
            }
        }

        public bool IsNotEmpty
        {
            get
            {
                return fullPath != null;
            }
        }

        public bool DirectoryExists
        {
            get
            {
                return fullPath != null && System.IO.Directory.Exists(new System.IO.FileInfo(fullPath).Directory.FullName);
            }

        }

        public Directory Directory
        {
            get
            {
                return new Directory(fullPath != null ? new System.IO.FileInfo(fullPath).Directory : null);
            }
        }

        public string DirectoryName
        {
            get
            {
                return fullPath != null ? new System.IO.FileInfo(fullPath).DirectoryName : "";
            }
        }

        public static File operator +(File a, string ext)
        {
            return new File(a.FullName + ext);
        }

        public string CoreName
        {
            get
            {
                if (fullPath == null)
                    return null;
                var fi = new System.IO.FileInfo(fullPath);
                return fi.Name.Substring(0, fi.Name.IndexOf(fi.Extension));
            }
        }
        public override int GetHashCode()
        {
            return fullPath != null ? fullPath.GetHashCode() : 0;
        }
        public override bool Equals(object obj)
        {
            return obj is File && ((File)obj) == this;
        }

        public static bool operator ==(File a, File b)
        {
            return a.fullPath == b.fullPath;
        }
        public static bool operator !=(File a, File b)
        {
            return a.fullPath != b.fullPath;
        }

        public string Text
        {
            get
            {
                return fullPath != null ? System.IO.File.ReadAllText((fullPath)) : null;
            }
            set
            {
                if (fullPath == null)
                    return;
                System.IO.File.WriteAllText(fullPath, value);
            }
        }
    }



    public struct Directory
    {
        string fullPath;

        public Directory(string path)
        {
            fullPath = path != null ? new System.IO.DirectoryInfo(path).FullName : null;
        }

        public Directory(DirectoryInfo directoryInfo)
        {
            fullPath = directoryInfo != null ? directoryInfo.FullName : null;
        }

        public string FullName
        {
            get
            {
                return fullPath;
            }
        }

        public bool Exists
        {
            get
            {
                return fullPath != null && System.IO.Directory.Exists(fullPath);
            }
        }

        public bool IsEmpty
        {
            get
            {
                return fullPath == null;
            }
        }

        public bool IsNotEmpty
        {
            get
            {
                return fullPath != null;
            }
        }

        public bool ParentExists
        {
            get
            {
                return fullPath != null && System.IO.Directory.Exists(new System.IO.DirectoryInfo(fullPath).Parent.FullName);
            }

        }

        public System.IO.DirectoryInfo Parent
        {
            get
            {
                return fullPath != null ? new System.IO.DirectoryInfo(fullPath).Parent : null;
            }
        }

        public string ParentName
        {
            get
            {
                return fullPath != null ? new System.IO.DirectoryInfo(fullPath).Parent.Name : "";
            }
        }

        public string Name
        {
            get
            {
                if (fullPath == null)
                    return null;
                var fi = new System.IO.DirectoryInfo(fullPath);
                return fi.Name;
            }
        }
        public override int GetHashCode()
        {
            return fullPath != null ? fullPath.GetHashCode() : 0;
        }
        public override bool Equals(object obj)
        {
            return obj is Directory && ((Directory)obj) == this;
        }

        public static bool operator ==(Directory a, Directory b)
        {
            return a.fullPath == b.fullPath;
        }
        public static bool operator !=(Directory a, Directory b)
        {
            return a.fullPath != b.fullPath;
        }
        public File[] Files
        {
            get
            {
                string[] members = System.IO.Directory.GetFiles(this.fullPath);
                File[] rs = new File[members.Length];
                for (int i = 0; i < members.Length; i++)
                    rs[i] = new File(members[i]);
                return rs;
            }
        }

        public File[] GetFiles(string pattern)
        {
            string[] members = System.IO.Directory.GetFiles(this.fullPath,pattern);
            File[] rs = new File[members.Length];
            for (int i = 0; i < members.Length; i++)
                rs[i] = new File(members[i]);
            return rs;
        }

        public Directory[] Directories
        {
            get
            {
                string[] members = System.IO.Directory.GetDirectories(this.fullPath);
                Directory[] rs = new Directory[members.Length];
                for (int i = 0; i < members.Length; i++)
                    rs[i] = new Directory(members[i]);
                return rs;
            }

        }
    }


    public partial class MainWindow : Form
	{
		public MainWindow()
		{
			InitializeComponent();
		}

        public static void BuildTree(TreeNodeCollection scope, Directory root, Directory dir)
        {
            File[] files = dir.GetFiles("*.txt");
            foreach (File f in files)
            {
                TreeNode node = scope.Add(f.CoreName);
                node.Tag = new DocumentTreeTag(root, f,node);
            }

            Directory[] subs = dir.Directories;
            foreach (Directory d in subs)
            {
                TreeNode node = scope.Add(d.Name);
                node.Tag = d;
                BuildTree(node.Nodes, root,d);
                node.Expand();
            }
        }

        Thread updateThread;

        private void MainWindow_Load(object sender, EventArgs e)
        {
            Directory root = new Directory("../documentation/documentation");
            BuildTree(documentTree.Nodes, root, root);
            updateThread = new Thread(new ThreadStart(ThreadMain));
            updateThread.Start();
        }

        private void documentTree_MouseUp(object sender, MouseEventArgs e)
        {
            TreeNode node = documentTree.SelectedNode;
            if (node.Tag is DocumentTreeTag)
            {
                SelectDocument((DocumentTreeTag)node.Tag);
            }
        }

        
        int textVersion = -1;
        private DocumentTreeTag lastTag=null;

        private void SelectDocument(DocumentTreeTag tag)
        {
            if (tag == lastTag)
                return;
            if (lastTag != null)
            {
                lastTag.Update(textEditor.Text);
            }
            documentName.Text = tag.Name;
            textEditor.Text = tag.Text;
            lastTag = tag;
            Interlocked.Increment(ref textVersion);
        }

        private void textEditor_TextChanged(object sender, EventArgs e)
        {
            Interlocked.Increment(ref textVersion);
            if (lastTag != null)
                lastTag.Update(textEditor.Text);
        }

        public delegate void Refresh(File path);

        private void RefreshBrowser(File path)
        {
            if (webBrowser.InvokeRequired)
            {
                webBrowser.Invoke(new Refresh(RefreshBrowser), new object[] { path });
                return;
            }
            webBrowser.Navigate(path.FullName);
            //webBrowser.DocumentText = html;
        }
       // "E:\projects\Hexaverse\documentation\html.php" "home.txt" > C:\Users\IronFox\AppData\Local\Temp\tmp96B7.tmp
        private void ThreadMain()
        {
            int myVersion = -2;
            Process myProcess = null;
            DocumentTreeTag myLastTag = null;
            File scriptOut = new File("../documentation/last.html");
            File builderScript = new File("../documentation/html.php");
            for (;;)
            {
                Thread.Sleep(1000);
                DocumentTreeTag tag = lastTag;
                if (tag == null || (!tag.Write() && myLastTag == tag))
                    continue;
                myLastTag = tag;
                myProcess = new Process();
                myProcess.StartInfo.UseShellExecute = false;
                myProcess.StartInfo.RedirectStandardOutput = true;
                myProcess.StartInfo.RedirectStandardError = true;
                myProcess.StartInfo.CreateNoWindow = true;
                myProcess.StartInfo.FileName = "php.exe"; //not the full application path
                myProcess.StartInfo.Arguments = "\""+builderScript.FullName+"\" \"" + tag.RelativePath + "\"";
                myProcess.StartInfo.WorkingDirectory = builderScript.DirectoryName;
                try
                {
                    if (!myProcess.Start())
                        throw new Exception("Failed to start process");
                    string output = myProcess.StandardOutput.ReadToEnd();
                    string error = myProcess.StandardError.ReadToEnd();
                    myProcess.WaitForExit();
                    scriptOut.Text = output;
                    RefreshBrowser(scriptOut);

                }
                catch (Exception e)
                {
                    bool brk2 = true;  
                }


            }
        }


        private void updater_Tick(object sender, EventArgs e)
        {
            
        }
    }



    public class DocumentTreeTag
    {
        private File f;
        private string content = null;
        private bool changed = false;
        private TreeNode node;
        private int version = 0;
        public readonly string RelativePath;

        public DocumentTreeTag(Directory root, File f, TreeNode node)
        {
            this.f = f;
            this.node = node;
            RelativePath = f.FullName.Substring(root.FullName.Length + 1);
        }

        public string Text
        {
            get
            {
                if (content != null)
                    return content;
                content = f.Text;
                return content;
            }
        }
        public bool Write()
        {
            lock (this)
            {
                if (!changed)
                    return false;
                f.Text = content;
                changed = false;
              //  node.Text = node.Text.Substring(1);
                return true;
            }
        }
        
        public string Name
        {
            get
            {
                return node.Text;
            }
        }

        internal void Update(string current)
        {
            lock (this)
            {
                if (current != content)
                {
                    content = current;
                    SignalChange();
                }
            }
        }

        public int Version
        {
            get
            {
                return version;
            }
        }

        internal void SignalChange()
        {
            System.Threading.Interlocked.Increment(ref version);
            if (!changed)
            {
                changed = true;
                //node.Text = "*" + node.Text;
            }
        }
    };
}
