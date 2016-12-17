using System.IO;
using System.Windows.Forms;

namespace DocumentationEditor
{
    public class Document
    {
        private File f;
        private string content = null;
        private bool changed = false;
        public readonly TreeNode Node;
        private int version = 0;
        public readonly string DocumentID;
        public readonly FolderInfo Parent;

        public Document(Directory root, FolderInfo folder, File f)
        {
            Node = folder.Scope.Add(f.CoreName);
            Node.Tag = this;

            this.f = f;
            DocumentID = Path.Combine(f.DirectoryName, f.CoreName).Substring(root.FullName.Length + 1);
            Parent = folder;
            Parent.Documents.Add(this);
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
                //   Debug.Assert(content.Length != 0);
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
                return Node.Text;
            }
        }

        internal void Update(string newContent)
        {
            lock (this)
            {
                if (newContent != content)
                {
                    content = newContent;
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

        internal void Remove()
        {
            Parent.Documents.Remove(this);
            Node.Remove();
            f.Remove();
            changed = false;
            f = new File();
        }
    };

}