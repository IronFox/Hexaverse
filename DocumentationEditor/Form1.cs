using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
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
        public override string ToString()
        {
            return fullPath ?? "<null>";
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
        static string[] reserved = { "CON", "PRN", "AUX", "NUL", "AUX.txt", ".", ".." };

        public static void VerifyIsValidName(string name)
        {
            name = name.ToUpper();
            for (int i = 1; i <= 9; i++)
            {
                if (name == "COM" + i)
                    throw new Exception("'"+name+"' is a reserved system name");
                if (name == "LPT" + i)
                    throw new Exception("'" + name + "' is a reserved system name");
            }
            foreach (string str in reserved)
                if (str == name)
                    throw new Exception("'" + name + "' is a reserved system name");

            if (name.IndexOfAny(Path.GetInvalidFileNameChars()) != -1)
                throw new Exception("'" + name + "' contains invalid characters");

            if (name.Length < 2)
                throw new Exception("'" + name + "' is shorter than 2 characters");
            if (name.Length > 64)
                throw new Exception("'" + name + "' is longer than 64 characters");
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

        public string Extension
        {
            get
            {
                if (fullPath == null)
                    return null;
                return new System.IO.FileInfo(fullPath).Extension;
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
                return Exists ? System.IO.File.ReadAllText(fullPath) : "";
            }
            set
            {
                if (fullPath == null)
                    return;
                System.IO.File.WriteAllText(fullPath, value);
            }
        }

		public DateTime LastWrite
		{
			get
			{
				if (!Exists)
					return new DateTime(1, 1, 1);
				return new System.IO.FileInfo(fullPath).LastWriteTimeUtc;
			}
		}

        public void Remove()
        {
            if (Exists)
                System.IO.File.Delete(fullPath);
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
        public override string ToString()
        {
            return fullPath ?? "<null>";
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

        public Directory Parent
        {
            get
            {
                return fullPath != null ? new Directory(new System.IO.DirectoryInfo(fullPath).Parent) : new Directory();
            }
        }

        public System.IO.DirectoryInfo Info
        {
            get
            {
                return fullPath != null ? new System.IO.DirectoryInfo(fullPath) : null;
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
        public static void VerifyIsValidName(string name)
        {
            File.VerifyIsValidName(name);
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
        /// <summary>
        /// Creates a new child directory or returns one if it already exists
        /// </summary>
        /// <param name="v"></param>
        /// <returns></returns>
        public Directory CreateSubdirectory(string v)
        {
            if (fullPath == null)
                throw new Exception("Cannot create sub-directory '"+v+"' of undefined directory reference");
            string subPath = Path.Combine(FullName, v);
            if (System.IO.Directory.Exists(subPath))
                return new Directory(subPath);
            return new Directory(Info.CreateSubdirectory(v));
        }

        public void Remove()
        {
            if (Exists)
                System.IO.Directory.Delete(fullPath);
        }
    }


    public partial class MainWindow : Form
	{
        //http://stackoverflow.com/questions/28217411/c-sharp-disable-webbrowser-sound-application-sound
        private const int FEATURE_DISABLE_NAVIGATION_SOUNDS = 21;
        private const int SET_FEATURE_ON_THREAD = 0x00000001;
        private const int SET_FEATURE_ON_PROCESS = 0x00000002;
        private const int SET_FEATURE_IN_REGISTRY = 0x00000004;
        private const int SET_FEATURE_ON_THREAD_LOCALMACHINE = 0x00000008;
        private const int SET_FEATURE_ON_THREAD_INTRANET = 0x00000010;
        private const int SET_FEATURE_ON_THREAD_TRUSTED = 0x00000020;
        private const int SET_FEATURE_ON_THREAD_INTERNET = 0x00000040;
        private const int SET_FEATURE_ON_THREAD_RESTRICTED = 0x00000080;

        // Necessary dll import
        [DllImport("urlmon.dll")]
        [PreserveSig]
        [return: MarshalAs(UnmanagedType.Error)]
        static extern int CoInternetSetFeatureEnabled(
        int FeatureEntry,
        [MarshalAs(UnmanagedType.U4)] int dwFlags,
        bool fEnable);

        public MainWindow()
		{
			InitializeComponent();
		}


        //Dictionary<string, Document> documentList = new Dictionary<string, Document>();


        public FolderInfo RegisterSubdirectory(FolderInfo parent, Directory child)
        {
            return (parent.RegisterChild(child));
        }

        public void BuildTree(FolderInfo root, FolderInfo dir)
        {
            File[] files = dir.Directory.GetFiles("*.txt");
            foreach (File f in files)
            {
                RegisterFile(root.Directory,dir, f);
            }
            files = dir.Directory.GetFiles("*.prefix");
            foreach (File f in files)
            {
                RegisterFile(root.Directory, dir, f);
            }

            Directory[] subs = dir.Directory.Directories;
            foreach (Directory d in subs)
            {
                FolderInfo sub = RegisterSubdirectory(dir, d);
                BuildTree(root, sub);
            }
        }

        public Document RegisterFile(Directory root, FolderInfo folder, File f)
        {
            Document rs = folder.FindDocument(Document.GetNameOf(f));
            if (rs != null)
                return rs;
            rs = new Document(root, folder, f);
            //documentList.Add(tag.DocumentID.ToLower(), tag);
            return rs;
        }

        Thread updateThread;

        public FolderInfo DocumentRoot { get; private set; }

        private void MainWindow_Load(object sender, EventArgs e)
        {
            CoInternetSetFeatureEnabled(FEATURE_DISABLE_NAVIGATION_SOUNDS, SET_FEATURE_ON_PROCESS, true);

            DocumentRoot = new FolderInfo(new Directory("../documentation/documentation"), documentTree.Nodes);
            FolderInfo root = DocumentRoot;
            try
            {
                BuildTree(root, root);
            }
            catch (Exception ex)
            {
                Log(ex);
            }
            updateThread = new Thread(new ThreadStart(ThreadMain));
            updateThread.Start();
        }



        
        private Document currentDocument=null;
        bool lockOnChange = false;

        private void SelectDocument(Document newDocument, bool force=false)
        {
            if (newDocument == currentDocument && !force)
                return;
            if (currentDocument != null && newDocument != currentDocument)
            {
                currentDocument.Update(textEditor.Text);
            }
            documentName.Text = newDocument != null ? newDocument.DocumentID : "";
            lockOnChange = true;
            textEditor.Text = newDocument != null ? newDocument.Text : "";
            lockOnChange = false;
            currentDocument = newDocument;
        }

        private void textEditor_TextChanged(object sender, EventArgs e)
        {
            if (lockOnChange)
                return;
            if (currentDocument != null)
                currentDocument.Update(textEditor.Text);
        }

        public delegate void DRefresh(File path);

        private void RefreshBrowser(File path)
        {
            if (webBrowser.InvokeRequired)
            {
                webBrowser.Invoke(new DRefresh(RefreshBrowser), new object[] { path });
                return;
            }
            //Log("Refreshing browser to '" + path + "'");
            webBrowser.Navigate(path.FullName);
            //webBrowser.DocumentText = html;
        }
       // "E:\projects\Hexaverse\documentation\html.php" "home.txt" > C:\Users\IronFox\AppData\Local\Temp\tmp96B7.tmp
        private void ThreadMain()
        {
            //int myVersion = -2;
            Process myProcess = null;
            Document myLastTag = null;
            File scriptOut = new File("../documentation/last.html");
            File builderScript = new File("../documentation/html.php");
            for (;;)
            {
                Thread.Sleep(1000);
                Document tag = currentDocument;
                if (tag == null || (!tag.Write() && myLastTag == tag))
                    continue;
                myLastTag = tag;
                myProcess = new Process();
                myProcess.StartInfo.UseShellExecute = false;
                myProcess.StartInfo.RedirectStandardOutput = true;
                myProcess.StartInfo.RedirectStandardError = true;
                myProcess.StartInfo.CreateNoWindow = true;
                myProcess.StartInfo.FileName = "php.exe"; //not the full application path
                myProcess.StartInfo.Arguments = " -r \"date_default_timezone_set('Europe/Berlin'); $_GET['page']='" + tag.DocumentID + "'; require '" + builderScript.FullName+"';\"";
                myProcess.StartInfo.WorkingDirectory = builderScript.DirectoryName;
                try
                {
					//Log("Invoking PHP ("+ myProcess.StartInfo.Arguments+")");
					try
					{
						if (!myProcess.Start())
							throw new Exception("Failed to start PHP process. Make sure PHP is installed and correctly configured in the path variable");
					}
					catch (Exception ex)
					{
						throw new Exception("Failed to start PHP process. Make sure PHP is installed and correctly configured in the path variable", ex);
					}
					string output = myProcess.StandardOutput.ReadToEnd();
                    string error = myProcess.StandardError.ReadToEnd();

                    string head = "<head>";
                    int headAt = output.IndexOf(head);
                    if (headAt != -1)
                    {
                        headAt += head.Length;
                        output = output.Substring(0, headAt) + "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\" />" + output.Substring(headAt);
                    }
                        
                    myProcess.WaitForExit();
                    scriptOut.Text = output;
                    RefreshBrowser(scriptOut);

                }
                catch (Exception e)
                {
                    Log(e);
                }
            }
        }


		private void updater_Tick(object sender, EventArgs e)
		{
			try
			{
				if (currentDocument != null && !currentDocument.CheckContent())
				{
					Log("File content changed and reloaded: " + currentDocument.Name);
					SelectDocument(currentDocument, true);
				}
			}
			catch (Exception ex)
			{
				Log(ex);
			}
        }


        public delegate void LogEntry(string line);

        internal void Log(string msg)
        {
            if (logBox.InvokeRequired)
            {
                logBox.Invoke(new LogEntry(Log), new object[] { msg });
                return;
            }

            logBox.AppendText("\r\n" + msg);
        }
        internal void Log(Exception ex)
        {
            Log("Exception caught: " + ex);
        }

        private void webBrowser_Navigating(object sender, WebBrowserNavigatingEventArgs e)
        {
            if (e.Url.AbsoluteUri.EndsWith("last.html"))
                return;
            e.Cancel = true;

            string needle = "html.php?page=";
            int at = e.Url.AbsoluteUri.IndexOf(needle);
            if (at == -1)
            {
                Log("URI not supported by editor: "+e.Url.AbsoluteUri+". Ignoring");
                return;
            }
            string site = e.Url.AbsoluteUri.Substring(at + needle.Length);
            site = site.Replace("%20", " ");
            site = site.ToLower();
            //while ((at = site.IndexOf("  ")) != -1)
            //    site = site.Remove(at, 1);
            //Document entry;
            //if (documentList.TryGetValue(site.ToLower(), out entry))
            //{
            //    documentTree.SelectedNode = entry.Node;
            //    return;
            //}

            try
            {
                FolderInfo folder = DocumentRoot;
                string[] segments = site.Split('/');
                for (int i = 0; i < segments.Length - 1; i++)
                {
                    FolderInfo sub = folder.FindFolder(segments[i]);
                    if (sub != null)
                    {
                        folder = sub;
                        continue;
                    }
                    Directory.VerifyIsValidName(segments[i]);
                    Directory dir = folder.Directory.CreateSubdirectory(segments[i]);
                    folder = RegisterSubdirectory(folder, dir);
                }
                string docName = segments[segments.Length - 1];
                Document d = folder.FindDocument(docName);
                if (d == null)
                {
                    File.VerifyIsValidName(docName);
                    File f = new File(Path.Combine(folder.Directory.FullName, docName + ".txt"));
                    d = RegisterFile(DocumentRoot.Directory, folder, f);
                }
                documentTree.SelectedNode = d.Node;
            }
            catch (Exception ex)
            {
                Log(ex);
            }
        }

        private void documentTree_AfterSelect(object sender, TreeViewEventArgs e)
        {
            TreeNode node = e.Node;
            if (node == null)
                return;
            if (node.Tag is Document)
            {
                //Log(node.Text);
                SelectDocument((Document)node.Tag);
            }
        }

        private void deleteToolStripMenuItem_Click(object sender, EventArgs e)
        {
            TreeNode n = documentTree.SelectedNode;
            if (n == null)
                return;
            Document doc = n.Tag as Document;
            FolderInfo dir = n.Tag as FolderInfo;
            if (doc == null && dir == null)
                return;
            string text;
            if (doc != null)
                text = "Please confirm deletion of document '" + doc.DocumentID + "'. This operation cannot be undone.";
            else
                text = "Please confirm deletion of directory '"+dir+ "'. All contained documents and subdirectories will be deleted. This operation cannot be undone.";

            var confirmResult = MessageBox.Show(text,
                                        "Confirmation required",
                                        MessageBoxButtons.YesNo);
            if (confirmResult == DialogResult.Yes)
            {
                try
                {
                    if (doc != null)
                    {
                        SelectDocument(null);
                        doc.Remove();
                        Log(doc.DocumentID + " removed");
                    }
                    else
                    {
                        dir.Remove();
                        Log(dir + " removed");
                    }
                }
                catch (Exception ex)
                {
                    Log(ex);
                }
            }
        }

        private void newFolderToolStripMenuItem_Click(object sender, EventArgs e)
        {
            TreeNode n = documentTree.SelectedNode;
            if (n == null)
                return;
            Document doc = n.Tag as Document;
            FolderInfo dir = n.Tag as FolderInfo;
            if (doc != null)
                dir = doc.Parent;
            Debug.Assert(dir != null);

            new CreateEntryForm().Show(this, dir, false);
        }

        private void MainWindow_FormClosed(object sender, FormClosedEventArgs e)
        {
            Environment.Exit(0);
        }

        private void newDocumentToolStripMenuItem_Click(object sender, EventArgs e)
        {
            TreeNode n = documentTree.SelectedNode;
            if (n == null)
                return;
            Document doc = n.Tag as Document;
            FolderInfo dir = n.Tag as FolderInfo;
            if (doc != null)
                dir = doc.Parent;
            Debug.Assert(dir != null);

            new CreateEntryForm().Show(this, dir, true);

        }

        private void documentContextMenu_Opening(object sender, CancelEventArgs e)
        {
            TreeNode n = documentTree.SelectedNode;
            if (n == null)
            {
                newDocumentToolStripMenuItem.Enabled = false;
                newFolderToolStripMenuItem.Enabled = false;
                deleteToolStripMenuItem.Enabled = false;
                newDocumentToolStripMenuItem.Text = "Create document...";
                newFolderToolStripMenuItem.Text = "Create folder...";
                deleteToolStripMenuItem.Text = "Delete";
                return;
            }
            if (n.Tag is Document)
            {
                newDocumentToolStripMenuItem.Enabled = true;
                newDocumentToolStripMenuItem.Text = "Create document in '" + ((Document)n.Tag).ParentNiceName + "'...";
                newFolderToolStripMenuItem.Enabled = true;
                newFolderToolStripMenuItem.Text = "Create folder in '" + ((Document)n.Tag).ParentNiceName + "'...";
                deleteToolStripMenuItem.Enabled = true;
                deleteToolStripMenuItem.Text = "Delete '"+ ((Document)n.Tag).DocumentID+"'";
            }
            else if (n.Tag is FolderInfo)
            {
                newDocumentToolStripMenuItem.Enabled = true;
                newDocumentToolStripMenuItem.Text = "Create document in '" + ((FolderInfo)n.Tag).NiceName + "'...";
                newFolderToolStripMenuItem.Enabled = true;
                newFolderToolStripMenuItem.Text = "Create folder in '" + ((FolderInfo)n.Tag).NiceName + "'...";
                deleteToolStripMenuItem.Enabled = true;
                deleteToolStripMenuItem.Text = "Delete '" + ((FolderInfo)n.Tag).NiceName + "'";
            }
            else
            {
                newDocumentToolStripMenuItem.Enabled = false;
                newFolderToolStripMenuItem.Enabled = false;
                deleteToolStripMenuItem.Enabled = false;
                newDocumentToolStripMenuItem.Text = "Create document...";
                newFolderToolStripMenuItem.Text = "Create folder...";
                deleteToolStripMenuItem.Text = "Delete";
                Log("Clicked on unexpected tree node '" + n.Text + "'");
            }
        }

        private void documentTree_MouseUp(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                // Select the clicked node
                documentTree.SelectedNode = documentTree.GetNodeAt(e.X, e.Y);

                if (documentTree.SelectedNode != null)
                {
                    documentContextMenu.Show(documentTree, e.Location);
                }
            }
        }
    }



}
