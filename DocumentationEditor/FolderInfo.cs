using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Windows.Forms;

namespace DocumentationEditor
{
    public class FolderInfo
    {
        public readonly Directory Directory;
        public readonly TreeNode Node;
        public readonly TreeNodeCollection Scope;
        public readonly FolderInfo Parent;
        public List<FolderInfo> Folders = new List<FolderInfo>();
        public List<Document> Documents = new List<Document>();
        public readonly string NiceName;

        public bool Removed { get; private set; }

        public FolderInfo(Directory dir, TreeNodeCollection scope)
        {
            Directory = dir;
            Scope = scope;
            Node = null;
            Parent = null;
            NiceName = "/";
        }
        private FolderInfo(FolderInfo parent, Directory dir)
        {
            Directory = dir;
            string dirName = GetNameOf(dir);
            Node = parent.Scope.Add(dirName);
            Scope = Node.Nodes;
            Node.Tag = this;
            Parent = parent;
            Node.Expand();
            parent.Folders.Add(this);
            NiceName = parent.NiceName + dirName;
        }



        public override string ToString()
        {
            return NiceName;
        }

        public void Remove()
        {
            for (int i = Folders.Count - 1; i >= 0; i--)
                Folders[i].Remove();
            if (Folders.Count > 0)
                throw new Exception("All folders should be removed");
            for (int i = Documents.Count - 1; i >= 0; i--)
                Documents[i].Remove();
            if (Documents.Count > 0)
                throw new Exception("All documents should be removed");

            if (Node != null)
                Node.Remove();

            if (Directory.Exists)
                Directory.Remove();
            if (Parent != null)
                Parent.Folders.Remove(this);
            Removed = true;
        }

        internal FolderInfo FindFolder(string text)
        {
            if (!text.EndsWith("/"))
                text += '/';
            text = text.ToLower();
            foreach (var folder in Folders)
                if (folder.Node.Text.ToLower() == text)
                    return folder;
            return null;
        }
        internal Document FindDocument(string text)
        {
            text = text.ToLower();
            foreach (var doc in Documents)
                if (doc.Name.ToLower() == text)
                    return doc;
            return null;
        }

        internal static string GetNameOf(Directory child)
        {
            return child.Name + '/';
        }

        internal FolderInfo RegisterChild(Directory child)
        {
            foreach (var chld in Folders)
                if (chld.Directory == child)
                    return chld;
            return new FolderInfo(this, child);
        }
    }
}