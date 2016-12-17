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
        static string[] reserved = { "CON", "PRN", "AUX", "NUL", "AUX.txt" };

        public static void VerifyIsValidName(string name)
        {
            name = name.ToUpper();
            for (int i = 1; i <= 9; i++)
            {
                if (name == "COM" + i)
                    throw new Exception("Filename '"+name+"' is a reserved system name");
                if (name == "LPT" + i)
                    throw new Exception("Filename '" + name + "' is a reserved system name");
            }
            foreach (string str in reserved)
                if (str == name)
                    throw new Exception("Filename '" + name + "' is a reserved system name");

            if (name.IndexOfAny(Path.GetInvalidFileNameChars()) != -1)
                throw new Exception("Filename '" + name + "' contains invalid characters");

            if (name.Length < 2)
                throw new Exception("Filename '" + name + "' is shorter than 2 characters");
            if (name.Length > 64)
                throw new Exception("Filename '" + name + "' is longer than 64 characters");
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
                return Exists ? System.IO.File.ReadAllText(fullPath) : "";
            }
            set
            {
                if (fullPath == null)
                    return;
                System.IO.File.WriteAllText(fullPath, value);
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
             if (name.IndexOfAny(Path.GetInvalidPathChars()) != -1)
                throw new Exception("Directory name '" + name + "' contains invalid characters");
            if (name.Length < 2)
                throw new Exception("Directory name '" + name + "' is shorter than 2 characters");
            if (name.Length > 64)
                throw new Exception("Directory name '" + name + "' is longer than 64 characters");
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


        Dictionary<string, Document> documentList = new Dictionary<string, Document>();


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

            Directory[] subs = dir.Directory.Directories;
            foreach (Directory d in subs)
            {
                FolderInfo sub = RegisterSubdirectory(dir, d);
                BuildTree(root, sub);
            }
        }

        private Document RegisterFile(Directory root, FolderInfo folder, File f)
        {
            Document tag = new Document(root, folder, f);
            documentList.Add(tag.DocumentID.ToLower(), tag);
            return tag;
        }

        Thread updateThread;

        public FolderInfo DocumentRoot { get { return new FolderInfo (new Directory("../documentation/documentation"), documentTree.Nodes); } }

        private void MainWindow_Load(object sender, EventArgs e)
        {
            CoInternetSetFeatureEnabled(FEATURE_DISABLE_NAVIGATION_SOUNDS, SET_FEATURE_ON_PROCESS, true);

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

        private void documentTree_MouseUp(object sender, MouseEventArgs e)
        {

        }

        
        private Document lastTag=null;
        bool lockOnChange = false;

        private void SelectDocument(Document tag)
        {
            if (tag == lastTag)
                return;
            if (lastTag != null)
            {
                lastTag.Update(textEditor.Text);
            }
            documentName.Text = tag != null ? tag.DocumentID : "";
            lockOnChange = true;
            textEditor.Text = tag != null ? tag.Text : "";
            lockOnChange = false;
            lastTag = tag;
        }

        private void textEditor_TextChanged(object sender, EventArgs e)
        {
            if (lockOnChange)
                return;
            if (lastTag != null)
                lastTag.Update(textEditor.Text);
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
                Document tag = lastTag;
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
                    if (!myProcess.Start())
                        throw new Exception("Failed to start process");
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
            
        }


        public delegate void LogEntry(string line);

        private void Log(string msg)
        {
            if (logBox.InvokeRequired)
            {
                logBox.Invoke(new LogEntry(Log), new object[] { msg });
                return;
            }

            logBox.AppendText("\r\n" + msg);
        }
        private void Log(Exception ex)
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
            Document entry;
            if (documentList.TryGetValue(site.ToLower(), out entry))
            {
                documentTree.SelectedNode = entry.Node;
                return;
            }

            try
            {
                FolderInfo folder = DocumentRoot;
                string[] segments = site.Split('/');
                for (int i = 0; i < segments.Length - 1; i++)
                {
                    Directory.VerifyIsValidName(segments[i]);
                    Directory dir = folder.Directory.CreateSubdirectory(segments[i]);
                    folder = RegisterSubdirectory(folder, dir);
                }
                string docName = segments[segments.Length - 1];
                File.VerifyIsValidName(docName);
                File f = new File(Path.Combine(folder.Directory.FullName, docName+".txt"));

                var d= RegisterFile(DocumentRoot.Directory, folder, f);
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
                        documentList.Remove(doc.DocumentID.ToLower());
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

        }
    }



}
