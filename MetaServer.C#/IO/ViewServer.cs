using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Net.Sockets;
using System.Xml.Linq;
using System.IO;
using System.Diagnostics;

namespace Consensus
{
    public class ViewServer
    {
        protected static ReaderWriterLock accessLock = new ReaderWriterLock();


        public static class Config
        {
            public static string SVNExecutable { get; private set; }
            public static string GitExecutable { get; private set; }
            public static ProgramMode ProgramMode { get; private set; }

            internal static void Init(XElement xrep, ProgramMode mode)
            {
                SVNExecutable = xrep.Element("svn").Value;
                GitExecutable = xrep.Element("git").Value;
                Config.ProgramMode = mode;
            }
        }


        public class GlobalView
        {
            public UInt32 GlobalID;
            public Account Account;
            public Structure StructureReference;
            public Vec2 DocumentSize,ViewSize,ViewOffset;
            public float RelativePosition;
        }

        private static string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,._-+=:;<>/\\?!~`@#$%^&*()|{}[]";

        private static int PwCharToInt(char c)
        {
            return characters.IndexOf(c);
        }

        private static char PwIntToChar(int i)
        {
            return characters[i];
        }

        public static string RotatePassword(string password)
        {
            char[] field = new char[password.Length];
            for (int i = 0; i < password.Length; i++)
                field[i] = PwIntToChar((PwCharToInt(password[i])+characters.Length/2)%characters.Length);
            return new string(field);
        }

        public class Repository
        {
            public string   Type;
            public string   URL,
                            Branch,
                            UserName,
                            Password;


            public XElement ToXML()
            {
                XElement result = new XElement("repository");
                result.Set("url", URL);
                result.Set("type", Type);
                if (Branch != null && Branch.Length > 0)
                    result.Set("branch", Branch);
                if (UserName != null && UserName.Length > 0)
                    result.Set("username", UserName);
                if (Password != null && Password.Length > 0)
                    result.Set("password", RotatePassword(Password));

                return result;
            }

            public Repository(XElement xml)
            {
                URL = xml.GetString("url");
                Type = xml.GetString("type");
                Branch = xml.GetOptionalString("branch");
                UserName = xml.GetOptionalString("username");
                Password = RotatePassword(xml.GetOptionalString("password"));
                if (Password.Length == 0)
                    Password = xml.GetOptionalString("raw_password");
            }
        }

        public class Scope : IComparable
        {
            public static int Count = 0;
            public Scope Parent;
            public Repository Repository;
            public List<Scope> Children = new List<Scope>();
            public Structure Structure;
            public String Name;
            public bool IsFile = false;
            public int FileLength=0,
                        FileLines = 0;
            public float Radius = 0,
                        Height = 0;
            public Vec3 CentralPosition;

            public const float LayerHeight = 10.0f;


	        public class Ring
	        {
		        public float			Radius,
								        FillLevel,
								        MaxMemberRadius;
		        public List<Scope>      Elements = new List<Scope>();
	        };

	        public List<Ring>			Rings = new List<Ring>();

            public XElement ToXML()
            {
                if (Repository != null)
                    return Repository.ToXML();
                return null;
            }

            public Scope()
            {
                Count++;
            }


            public int CompareTo(object obj)
            {
                Scope other = obj as Scope;
                if (other != null)
                {
                    if (Height < other.Height)
                        return 1;
                    if (Height > other.Height)
                        return -1;
                    return 0;
                }
                return 0;

            }

            public Scope FindOrCreateChild(string element)
            {
                foreach (Scope scope in Children)
                    if (scope.Name == element)
                        return scope;
                Scope newScope = new Scope();
                newScope.Name = element;
                newScope.Parent = this;
                Children.Add(newScope);
                return newScope;
            }

            public Scope CreateChild(string name)
            {
                foreach (Scope scope in Children)
                    if (scope.Name == name)
                    {
                        Debug.Fail("Element '" + name + "' already exists");
                        return null;
                    }
                Scope newScope = new Scope();
                newScope.Name = name;
                newScope.Parent = this;
                Children.Add(newScope);
                return newScope;

            }


            public void UpdateRecursiveLayoutB()
            {
                foreach (var r in Rings)
                {
                    float at = 0;
                    foreach (var el in r.Elements)
                    {
                        float step = el.Radius * 2.5f;
                        float angle = (at + step * 0.5f) / r.FillLevel * (float)Math.PI * 2.0f;
                        el.CentralPosition.x = CentralPosition.x + (float)Math.Cos(angle) * r.Radius;
                        el.CentralPosition.y = CentralPosition.y + (float)Math.Sin(angle) * r.Radius;
                        el.CentralPosition.z = CentralPosition.z + (Parent != null ? LayerHeight : 0.0f);
                        //el.CentralPosition.z = CentralPosition.z + LayerHeight;
                        at += step;
                    }
                }
                if (Rings.Count == 0)
                {
                    foreach (var s in Children)
                    {
                        s.CentralPosition = CentralPosition;
                        if (Parent != null)
                            s.CentralPosition.z += LayerHeight;
                    }
                }
                if (Parent != null)
                {
                    const float constantScale = 1.0f;
                    Vec3 c = CentralPosition * constantScale;
                    float r = Radius * constantScale;
                    float h = Children.Count > 0 ? LayerHeight : Height;
                        h*= constantScale;
                    Box bounds = new Box(c - new Vec3(r, r, 0.0f), c + new Vec3(r, r, h));
                    if (IsFile)
                        Structure = new Structure("City.File", bounds, false);
                    else
                        Structure = new Structure("City.Scope", bounds, false);
                    if (Parent != null && Parent.Structure != null)
                        Structure.IntAttributes["Parent"] = (int)Parent.Structure.ID.Value;
                    Structure.StringAttributes["Name"] = Name;

                    Structure.Collection.InsertNew(Structure);
                    Structure.BVH.Insert(Structure, Structure.BoundingVolume);
                    Structure.UpdateDescription();
                }
                foreach (var s in Children)
                    s.UpdateRecursiveLayoutB();
            }

            public void UpdateRecursiveLayoutA()
	        {
                if (Children.Count == 0)
                    return;
		        foreach (var s in Children)
			        s.UpdateRecursiveLayoutA();
		        if (Children.Count == 1)
		        {
			        Radius = Children[0].Radius;
			        Height = Children[0].Height;
                    if (Parent != null)
                        Height += LayerHeight;
			        return;
		        }
                Scope[] sorted = new Scope[Children.Count];
                for (int i = 0; i < sorted.Length; i++)
                    sorted[i] = Children[i];

                Array.Sort(sorted);
		        Radius = 0;
		        Height = 0;
		        int at = 0;
		        while (at < sorted.Length && sorted[at].Radius == 0)
			        at++;
		        if (at >= sorted.Length)
			        return;

		        const float scale = 1.2f;

		        Ring ring = new Ring();
                Rings.Add(ring);
		        Ring prev = null;
                ring.Radius = 0.0f;
                ring.FillLevel = 1.0f;
                ring.MaxMemberRadius = sorted[at].Radius;
                ring.Elements.Add(sorted[at]);
                Height = sorted[at].Height;
                at++;
		        //ring.Radius = sorted[at].Radius*2.0f;
		        //ring.FillLevel = 0;
		        //ring.MaxMemberRadius = 0;
		        while (at < sorted.Length)
		        {
			        Scope el = sorted[at++];
			        Height = Math.Max(Height,el.Height);
			        if (el.Radius == 0)
				        continue;
			        float step = el.Radius * 2.5f;
			        if (ring.FillLevel + step < 2.0f * (float)Math.PI * ring.Radius)
			        {
				        ring.Elements.Add(el);
				        ring.FillLevel += step;
                        ring.MaxMemberRadius = Math.Max(ring.MaxMemberRadius, el.Radius);
				        if (prev != null && prev.Radius + prev.MaxMemberRadius * scale > ring.Radius - ring.MaxMemberRadius * scale)
				        {
					        ring.Radius = prev.Radius + (prev.MaxMemberRadius + ring.MaxMemberRadius) * scale;
				        }
				        continue;
			        }
                    prev = ring;
                    ring = new Ring();
                    Rings.Add(ring);
			        ring.MaxMemberRadius = el.Radius;
			        ring.Radius = prev.Radius + (prev.MaxMemberRadius + ring.MaxMemberRadius) * scale;
			        ring.FillLevel = step;
			        ring.Elements.Add(el);
		        }
		        Radius = ring.Radius + ring.MaxMemberRadius;
		        Height += LayerHeight;
	        }
        }


        private static Dictionary<string, Scope> scopeMap = new Dictionary<string, Scope>();
        private static Scope root = new Scope();
        private static Dictionary<uint, GlobalView> allViews = new Dictionary<uint, GlobalView>();
        private static volatile UInt32 viewIDCounter = 0;
        private static HashSet<string> supportedExtensions = new HashSet<string>();

        private static string UnifyPath(string path)
        {
            return path.Replace('\\', '/');
        }

        static void Scan(Scope scope, string filePath)
        {
            using (StreamReader r = new StreamReader(filePath))
            {
                scope.FileLines = 0;
                scope.FileLength = 0;
                int chr;
                while ((chr = r.Read()) != -1)
                {
                    if ((byte)chr == (byte)'\n')
                        scope.FileLines++;
                    scope.FileLength++;
                }
            }
    		scope.Radius = (float) (scope.FileLength) / (float)(scope.FileLines+1)/10.0f;
		    scope.Height = (float)(scope.FileLines+1) / 20.0f;
        }

        private static void RecursivelyLoad(DirectoryInfo dir, Scope target, string path)
        {
            try
            {
                var files = dir.GetFiles();
                foreach (FileInfo file in files)
                {
                    if (supportedExtensions.Contains(file.Extension.ToLower()))
                    {
                        Scope child = target.CreateChild(file.Name);
                        child.IsFile = true;
                        Scan(child, file.FullName);
                        scopeMap[path + file.Name] = child;
                    }
                }
                var folders = dir.GetDirectories();
                foreach (DirectoryInfo folder in folders)
                {
                    Scope child = target.CreateChild(folder.Name);
                    child.IsFile = false;
                    RecursivelyLoad(folder, child,path+folder.Name+"/");
                }
            }
            catch (Exception e)
            {
                Program.ReportUnexpected(null, e, Significance.Unusual);
            }

        }

        public static string Encode(string url)
        {
            StringBuilder   builder = new StringBuilder();
            foreach (char c in url)
            {
                if (char.IsLetterOrDigit(c))
                    builder.Append(c);
                else
                    builder.Append('$').Append((int)c);
            }
            return builder.ToString();
        }

        public static void LoadRepository(XElement source)
        {
            if (Config.ProgramMode == ProgramMode.Game)
                return;
            if (supportedExtensions.Count == 0)
            {
                Program.Log(null, Significance.Low, "ViewServer: Setting up extension map");
                supportedExtensions.Add(".h");
                supportedExtensions.Add(".c");
                supportedExtensions.Add(".hpp");
                supportedExtensions.Add(".cpp");
                supportedExtensions.Add(".c++");
                supportedExtensions.Add(".java");
                supportedExtensions.Add(".php");
                supportedExtensions.Add(".cs");
                supportedExtensions.Add(".js");
                supportedExtensions.Add(".py");
                supportedExtensions.Add(".xml");
                supportedExtensions.Add(".txt");
            }
            Repository rep = new Repository(source);

            string rawID = rep.URL;
            if (rep.Type == "git")
            {
                string branch = rep.Branch;
                rawID += ':' + branch;
            }
            

            string id = Encode(rawID);


            Scope repositoryScope = root.FindOrCreateChild(rawID);
            repositoryScope.Repository = rep;
            
            string path = "repositories/"+rep.Type+"/"+id;
            Program.Log(null, Significance.Low, "ViewServer: Loading repository scope '"+rawID+"' into '" + path + "'");

            string wd = Directory.GetCurrentDirectory();
            Process p = new Process();
            p.StartInfo.CreateNoWindow = false;
            p.StartInfo.UseShellExecute = false;
            if (!Directory.Exists(path+"/."+rep.Type))  //.git / .svn folders exist in both cases, so useful
            {
                Directory.CreateDirectory(path);
                Directory.SetCurrentDirectory(path);


                if (rep.Type == "svn")
                {
                    Program.Log(null, Significance.Important, "ViewServer: Checking out SVN repository '" + rep.URL + "'");
                    p.StartInfo.FileName = Config.SVNExecutable;
                    p.StartInfo.Arguments = "checkout "+rep.URL+" . -q";
                    if (rep.UserName.Length > 0)
                        p.StartInfo.Arguments += " --username "+rep.UserName;
                    if (rep.Password.Length > 0)
                        p.StartInfo.Arguments += " --password "+rep.Password;

                }
                else
                    if (rep.Type == "git")
                    {
                        Program.Log(null, Significance.Important, "ViewServer: Cloning Git repository '" + rep.URL + "'");
                        p.StartInfo.FileName = Config.GitExecutable;
                        p.StartInfo.Arguments = "clone ";
                        if (rep.UserName.Length > 0)
                            p.StartInfo.Arguments += rep.UserName+"@";
                        p.StartInfo.Arguments += rep.URL+" -q -b "+rep.Branch+" .";
                    }
                    else
                        throw new NotImplementedException(rep.Type);

            }
            else
            {
                p = null;

                /*
                Directory.SetCurrentDirectory(path);

                if (rep.Type == "svn")
                {
                    Program.Log(null, Significance.Important, "ViewServer: Updating SVN repository '" + rep.URL + "'");
                    p.StartInfo.FileName = Config.SVNExecutable;
                    p.StartInfo.Arguments = "update -q";
                }
                else
                    if (rep.Type == "git")
                    {
                        Program.Log(null, Significance.Important, "ViewServer: Pulling Git repository '" + rep.URL + "'");
                        p.StartInfo.FileName = Config.GitExecutable;
                        p.StartInfo.Arguments = "pull";
                    }
                    else
                        throw new NotImplementedException(rep.Type);
                 */
            }
            if (p != null)
            {
                p.Start();
                p.WaitForExit();
            }
            Directory.SetCurrentDirectory(wd);


            RecursivelyLoad(new DirectoryInfo(path), repositoryScope, rawID + ":/");
        }

        /// <summary>
        /// Signals that loading has finished and a layout should be computed
        /// </summary>
        public static void FinishLoading()
        {
            if (Config.ProgramMode == ProgramMode.Game)
                return;
            Program.Log(null, Significance.Important, "ViewServer: Constructing layout.");
            Structure.BVH.LockReconstruction();
            root.CentralPosition = new Vec3(0.0f);
            root.UpdateRecursiveLayoutA();
            root.UpdateRecursiveLayoutB();
            Structure.BVH.UnlockReconstruction();
            Program.Log(null, Significance.Important, "ViewServer: All done. Allocated "+Scope.Count+" scopes.");
        }

        /// <summary>
        /// Clears all product/branch data. Does not affect clients or connections.
        /// Should only be called during initialization
        /// </summary>
        public static void ClearData()
        {
            Scope.Count = 0;
            scopeMap.Clear();
            root = new Scope();
        }

        public static void VisitProducts(Action<Scope> f)
        {
            foreach (var child in root.Children)
                f(child);
        }

        private static Structure FindStructure(string branchProject, string path)
        {
            //lock (projectMap) //readonly
            {
                Scope scope;
                if (!scopeMap.TryGetValue(branchProject + ':' + UnifyPath(path), out scope))
                {
                    Program.Log(null, Significance.Unusual, "ViewServer: Unable to locate file '" + branchProject + ':' + UnifyPath(path) + "'. This file has not been loaded during initialization.");
                    return null;
                }
                return scope.Structure;
            }

        }

        private static GlobalView CreateNewView(ByteBuffer buffer, string branchProject, string path, Account account)
        {
            GlobalView result = new GlobalView();
            result.Account = account;
            result.StructureReference = FindStructure(branchProject, path);
            Program.Log(null, Significance.Common, "ViewServer: Creating new view for client '"+account+"' to '" + branchProject+":"+path + "'");
            lock (allViews)
            {
                while (allViews.ContainsKey(viewIDCounter))
                    viewIDCounter++;
                result.GlobalID = viewIDCounter++;
                allViews[result.GlobalID] = result;
            }
            if (result.StructureReference != null)
                lock (Interface.In.Event.GetGlobalChangeAnchor())
                {
                    Interface.Out.Struct.ViewDescriptor desc = new Interface.Out.Struct.ViewDescriptor(account,result.GlobalID,result.StructureReference.ID.Value);
                    Interface.Out.Channel.OpenView.Broadcast(buffer, desc);
                }
            return result;
        }

        private static void RemoveView(ByteBuffer buffer, GlobalView view)
        {
            lock (allViews)
            {
                allViews.Remove(view.GlobalID);

            }
            if (view.StructureReference != null)
                lock (Interface.In.Event.GetGlobalChangeAnchor())
                {
                    Interface.Out.Channel.CloseView.Broadcast(buffer, view.GlobalID);
                }
        }


        public class Client
        {
            TcpClient tcpClient;
            Socket socket;
            Thread thread;
            ByteBuffer buffer = new ByteBuffer(256);

            Account account;
            Dictionary<int, GlobalView> viewMap = new Dictionary<int, GlobalView>();


            public Client(TcpClient tcpClient)
            {
                this.tcpClient = tcpClient;
                socket = tcpClient.Client;
                thread = new Thread(new ThreadStart(ThreadMain));
                thread.Start();
            }

            private void ThreadMain()
            {
                Program.Log(null, Significance.Common, "ViewServer: Client connected from '" + socket.RemoteEndPoint + "'");
                StringBuilder builder = new StringBuilder(128);
                try
                {
                    byte[] buffer = new byte[tcpClient.ReceiveBufferSize];
                    while (socket != null)
                    {
                        int len = socket.Receive(buffer);
                        if (len <= 0)
                            throw new Exception();
                        for (int i = 0; i < len; i++)
                        {
                            byte b = buffer[i];
                            if (b == 0)
                            {
                                lock (this)
                                    Interpret(builder.ToString());
                                builder.Clear();
                            }
                            else
                                builder.Append((char)b);
                        }
                    }
                }
                catch (ObjectDisposedException) //socket has been closed
                { }
                catch (SocketException) //probably safe, tpp
                { }
                catch (BlockException exception)
                {
                    Program.Log(null, Significance.ClientFatal, exception.Message);
                }
                catch (Exception e)
                {
                    Program.ReportUnexpected(null, e);
                }
                finally
                {
                    Close();
                    ViewServer.OnConnectionLoss(this);
                }
            }


            private void Interpret(string line)
            {
                if (socket == null)
                    return;
                string[] tokens = line.Split(' ');

                if (tokens.Length < 2)
                    return;

                string cmd = tokens[0];
	            if (cmd == "self")
	            {
                    account = Account.Collection.FindByName(tokens[1]);
                    if (account == null)
                        throw new ArgumentNullException("Unable to identify account: '"+tokens[1]+"'");
		            return;
	            }

	            int viewID;
                if (!int.TryParse(tokens[1],out viewID))
                    return;
                GlobalView view;
                if (!viewMap.TryGetValue(viewID,out view))
                {
                    if (cmd == "open")
                    {
                        if (tokens.Length < 4)
                            return;
                        view = CreateNewView(buffer,tokens[2],tokens[3],account);
                        viewMap[viewID] = view;
                        return;
                    }
                    else
                        return;
                }



                if (cmd == "close")
                {
                    RemoveView(buffer,view);
                    viewMap.Remove(viewID);
                    return;
                }

                if (cmd == "change")
                {
                    Interface.Out.Channel.ChangeView.Broadcast(buffer,view.GlobalID);
                    return;
                    //Interface.Out.Struct.ViewLocation   location = new Interface.Out.Struct.ViewLocation(viewID,relative);
                }
                if (cmd == "view")
                {
                    if (tokens.Length < 4)
                        return;
                    view.ViewOffset.x = Convert.ToFloat(tokens[2]);
                    view.ViewOffset.y = Convert.ToFloat(tokens[3]);
                    float pos = (view.ViewOffset.y + view.ViewSize.y * 0.5f ) / view.DocumentSize.y;
                    pos = Math.Max(0.0f,pos);
                    pos = Math.Min(1.0f,pos);
                    Interface.Out.Struct.ViewLocation location = new Interface.Out.Struct.ViewLocation(view.GlobalID,pos);

                    lock (Interface.In.Event.GetGlobalChangeAnchor())
                    {
                        view.RelativePosition = pos;
                        Interface.Out.Channel.UpdateViewLocation.Broadcast(buffer,location);
                    }
                    return;
                }
                if (cmd == "size")
                {
                    if (tokens.Length < 4)
                        return;
                    view.DocumentSize.x = Convert.ToFloat(tokens[2]);
                    view.DocumentSize.y = Convert.ToFloat(tokens[3]);
                    return;
                }
                if (cmd == "view_size")
                {
                    if (tokens.Length < 4)
                        return;
                    view.ViewSize.x = Convert.ToFloat(tokens[2]);
                    view.ViewSize.y = Convert.ToFloat(tokens[3]);
                    return;
                }
                if (cmd == "activate")
                {
                    //may need to add a lock here
                    Interface.Out.Channel.ActivateView.Broadcast(buffer,view.GlobalID);
                    return;
                }
                if (cmd == "deactivate")
                {
                    //may need to add a lock here
                    Interface.Out.Channel.DeactivateView.Broadcast(buffer,view.GlobalID);
                    return;
                }

                Program.Log(null,Significance.ClientFatal,"Received unrecognized view command type '"+cmd+"' from "+socket.RemoteEndPoint);
                Close();
            }

            public void Close()
            {
                lock (this)
                {
                    if (socket != null)
                    {
                        Program.Log(null, Significance.Common, "ViewServer: Closing connection to  '" + socket.RemoteEndPoint + "'");
                        Socket sock = socket;
                        socket = null;
                        sock.Close();
                        tcpClient.Close();
                        tcpClient = null;

                        foreach (var pair in viewMap)
                        {
                            RemoveView(buffer, pair.Value);
                        }
                        viewMap.Clear();

                    }
                }
            }
        }



        private static Thread   listenThread;
        private static TcpListener listener;
        private static List<Client> clients = new List<Client>();


        private static void AddNewClient(TcpClient client)
        {
            client.NoDelay = true;
            lock (clients)
            {
                Client myClient = new Client(client);
                clients.Add(myClient);
            }
        }



        private static void Listen()
        {
            Program.Log(null, Significance.Low, "Starting view listening thread on " + listener.LocalEndpoint.ToString());
            while (!Program.ShuttingDown)
            {
                try
                {
                    AddNewClient(listener.AcceptTcpClient());
                }
                catch (InvalidOperationException ex)
                {
                    if (!Program.ShuttingDown)
                        Program.ReportFatal(null, ex);
                    return;
                }
                catch (SocketException ex)
                {
                    if (!Program.ShuttingDown)
                        Program.ReportFatal(null, ex);
                    return;
                }
                catch (Exception ex)
                {
                    Program.ReportUnexpected(null, ex, Significance.Unusual);
                    return;
                }
            }

        }

        public static void Start(int port)
        {
            if (Config.ProgramMode == ProgramMode.Game)
                return;
            listener = new TcpListener(System.Net.IPAddress.Loopback, port); //IPv4
            listenThread = new Thread(new ThreadStart(Listen));

            listener.Start();
            listenThread.Start();

        }


        private static void OnConnectionLoss(Client client)
        {
            lock (clients)
            {
                clients.Remove(client);
            }
        }


        public static void End()
        {
            lock (clients)
            {
                if (listener != null)
                    listener.Stop();
                if (listenThread != null)
                    listenThread.Join();
                foreach (Client c in clients)
                {
                    c.Close();
                }
                clients.Clear();
            }
        }

        internal static void Init(XElement xconfig, ProgramMode mode)
        {
            Config.Init(xconfig.Element("repositories"),mode);


        }
    }
}
