using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace DocumentationEditor
{
    public class Document
    {
        private File file;
        private string content = "";
        private bool changed = false;
        public readonly TreeNode Node;
        private int version = 0;
        public readonly string DocumentID;
        public readonly FolderInfo Parent;
		private DateTime contentStamp = new DateTime(1,1,1);
		private bool havePreciseStamp = false;

        public static string GetNameOf(File f)
        {
            return f.CoreName;
        }

        public Document(Directory root, FolderInfo folder, File f)
        {
            Node = folder.Scope.Add(GetNameOf(f));
            Node.Tag = this;
			if (f.Extension == ".txt")
				file = f;
			else
			{
				file = new File(Path.Combine(f.DirectoryName, f.CoreName + ".txt"));
				Debug.Assert(!file.Exists);
			}

            DocumentID = Path.Combine(f.DirectoryName, f.CoreName).Substring(root.FullName.Length + 1);
			DocumentID = DocumentID.Replace('\\', '/');
            Parent = folder;
            Parent.Documents.Add(this);
        }

		/// <summary>
		/// Checks whether or not the actual file data has changed due to timestamp mismatch
		/// </summary>
		/// <returns>true if the filestamp is as expected, false if the local content has been updated from file</returns>
		public bool CheckContent()
		{
			var delta = contentStamp - file.LastWrite;
			if (Math.Abs(delta.TotalSeconds) < 5)
				return true;
			contentStamp = file.LastWrite;
			havePreciseStamp = true;
			content = file.Text;
			changed = false;
			return false;
		}

		public string Text
        {
            get
            {
				CheckContent();
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
				if (contentStamp != file.LastWrite && havePreciseStamp)
					return false;
                file.Text = content;
//				contentStamp = file.LastWrite;
				contentStamp = DateTime.UtcNow;
				havePreciseStamp = false;
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

        public string ParentNiceName { get { return Parent != null ? Parent.NiceName : "/"; } }

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
            file.Remove();
            changed = false;
            file = new File();
        }
    };

}