using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;
using Consensus.StorageItem;
using System.Threading;

namespace Consensus
{
    public class Structure : Entity, IDisposable
    {
        public static IDMap<Structure> Collection = new IDMap<Structure>();
        public static BoundingVolumeHierarchy<Structure> BVH = new BoundingVolumeHierarchy<Structure>();

        public static SyncList<Structure> RealtimeStructures = new SyncList<Structure>();

        public class RealtimeControl
        {
            public enum Flag
            {
            }

            private SpinLock myLock = new SpinLock();
            private int versionCounter = 0;
            private Structure parent = null;

            public Matrix4 System = new Matrix4(1);
            public Vec3 Momentum = new Vec3(0);
            public Vec3 LinearVelocity = new Vec3(0);
            public long Captured = 0;
            public ID ID {get {return parent.ID;} }
            private UInt32 flags = 0;

            public void Lock()
            {
                bool ack = false;
                myLock.Enter(ref ack);
                Debug.Assert(ack);
            }
            public void Release()
            {
                myLock.Exit();
            }

            public RealtimeControl(Structure target)
            {
                parent = target;
            }

            public bool IsOutdated(int version)
            {
                Lock();
                    bool result = versionCounter != version;
                Release();
                return result;
            }
            public int GetVersionNoLock()
            {
                return versionCounter;
            }
            public bool IsOutdatedNoLock(int version)
            {
                return versionCounter != version;
            }

            public int GetVersion()
            {
                Lock();
                    int result = versionCounter;
                Release();
                return result;
            }

            public uint GetFlags()
            {
                return flags;
                //return 0;
            }

            internal void Set(Matrix4 system, Vec3 velocity, Vec3 momentum, UInt32 flags, long captured)
            {
                Lock();
                {
                    System = system;
                    LinearVelocity = velocity;
                    Momentum = momentum;
                    this.flags = flags;
                    Captured = captured;

                    versionCounter++;
                }
                Release();
            }
        }


        public struct Attribute<T>
        {
            public string Name;
            public T Value;

            public Attribute(string name, T value)
            {
                Name = name;
                Value = value;
            }
        }


        public struct ContractBody
        {
            public string Type;
            public SortedList<string, int> IntAttributes,
                                        FloatAttributes,
                                        StringAttributes,
                                        BoolAttributes,
                                        Vec3Attributes,
                                        Vec4Attributes,
                                        Mat3Attributes,
                                        FileAttributes;

            public override int GetHashCode()
            {
                int hash = 17;
                hash = hash * 31 + Type.GetHashCode();
                hash = hash * 31 + HashCode(IntAttributes);
                hash = hash * 31 + HashCode(FloatAttributes);
                hash = hash * 31 + HashCode(StringAttributes);
                hash = hash * 31 + HashCode(BoolAttributes);
                hash = hash * 31 + HashCode(Vec3Attributes);
                hash = hash * 31 + HashCode(Vec4Attributes);
                hash = hash * 31 + HashCode(Mat3Attributes);
                hash = hash * 31 + HashCode(FileAttributes);
                return hash;
            }

            static int HashCode(SortedList<string, int> dict)
            {
                int hash = 17;
                foreach (var pair in dict)
                {
                    hash = hash * 31 + pair.Key.GetHashCode();
                    hash = hash * 31 + pair.Value.GetHashCode();
                }
                return hash;
            }

            static bool Eq(SortedList<string, int> a, SortedList<string, int> b)
            {
                if (a.Count != b.Count)
                    return false;
                foreach (var pair in a)
                {
                    int val;
                    if (!b.TryGetValue(pair.Key, out val) || val != pair.Value)
                        return false;
                }
                return true;
            }

            public override bool Equals(object obj)
            {
                if (!(obj is ContractBody))
                    return false;
                ContractBody other = (ContractBody)obj;
                return other.Type.Equals(Type)
                        && Eq(IntAttributes, other.IntAttributes)
                        && Eq(FloatAttributes,other.FloatAttributes)
                        && Eq(StringAttributes,other.StringAttributes)
                        && Eq(BoolAttributes,other.BoolAttributes)
                        && Eq(Vec3Attributes,other.Vec3Attributes)
                        && Eq(Vec4Attributes,other.Vec4Attributes)
                        && Eq(Mat3Attributes,other.Mat3Attributes)
                        && Eq(FileAttributes,other.FileAttributes);
            }

            public static bool operator== (ContractBody a, ContractBody b)
            {
               return a.Type.Equals(b.Type)
                        && Eq(a.IntAttributes, b.IntAttributes)
                        && Eq(a.FloatAttributes, b.FloatAttributes)
                        && Eq(a.StringAttributes, b.StringAttributes)
                        && Eq(a.BoolAttributes, b.BoolAttributes)
                        && Eq(a.Vec3Attributes, b.Vec3Attributes)
                        && Eq(a.Vec4Attributes, b.Vec4Attributes)
                        && Eq(a.Mat3Attributes, b.Mat3Attributes)
                        && Eq(a.FileAttributes, b.FileAttributes);
            }
            public static bool operator !=(ContractBody a, ContractBody b)
            {
                return !(a == b);
            }

            public static void PrintToLog(string typeName, SortedList<string, int> dict)
            {
                foreach (var pair in dict)
                {
                    Program.Log(null, Significance.Low, "  '" + pair.Key + "': " + typeName+" @" + pair.Value);
                }
                Program.Log(null, Significance.Low, "  Hash " + typeName + ": " + HashCode(dict));
            }
            public void PrintToLog()
            {
                Program.Log(null, Significance.Low, " '" + Type + "' (" + Type.GetHashCode()+")");
                PrintToLog("int",IntAttributes);
                PrintToLog("float", FloatAttributes);
                PrintToLog("string", StringAttributes);
                PrintToLog("bool", BoolAttributes);
                PrintToLog("vec3", Vec3Attributes);
                PrintToLog("vec4", Vec4Attributes);
                PrintToLog("mat3", Mat3Attributes);
                PrintToLog("file", FileAttributes);
            }
        }

        public class Contract
        {
            public class Violation : Exception
            {
                public Violation(string msg)
                    : base(msg)
                { }

            }

            public class Baked
            {
                public UInt16 Code;
                public string TypeName;
                public string[] IntAttributes,
                                FloatAttributes,
                                StringAttributes,
                                BoolAttributes,
                                Vec3Attributes,
                                Vec4Attributes,
                                Mat3Attributes,
                                FileAttributes;
            }

            public ContractBody Body;
            public static UInt16 CodeCounter = 0;
            public static object CodeLock = new object();
            public UInt16 Code;
            public Baked BakedVersion { get; private set; }


            private static string[] MakeList(SortedList<string, int> dict)
            {
                string[] result = new string[dict.Count];

                foreach (var pair in dict)
                    result[pair.Value] = pair.Key;

                return result;
            }

            public void UpdateBakedVersion()
            {
                BakedVersion = new Baked();
                BakedVersion.Code = Code;
                BakedVersion.TypeName = Body.Type;
                BakedVersion.IntAttributes = MakeList(Body.IntAttributes);
                BakedVersion.FloatAttributes = MakeList(Body.FloatAttributes);
                BakedVersion.StringAttributes = MakeList(Body.StringAttributes);
                BakedVersion.BoolAttributes = MakeList(Body.BoolAttributes);
                BakedVersion.Vec3Attributes = MakeList(Body.Vec3Attributes);
                BakedVersion.Vec4Attributes = MakeList(Body.Vec4Attributes);
                BakedVersion.Mat3Attributes = MakeList(Body.Mat3Attributes);
                BakedVersion.FileAttributes = MakeList(Body.FileAttributes);
            }
          
        }


        private static Dictionary<ContractBody, Contract> contracts = new Dictionary<ContractBody, Contract>();

        public class Descriptor
        {
            public UInt32 ID;
            public Box BoundingVolume;
            public UInt16 ContractCode;
            public int[] IntAttributes;
            public float[] FloatAttributes;
            public string[] StringAttributes;
            public bool[] BoolAttributes;
            public Vec3[] Vec3Attributes;
            public Vec4[] Vec4Attributes;
            public Matrix3[] Mat3Attributes;
            public UInt32[] FileAttributes;
        }


        public Box BoundingVolume;
        public string Type { get; private set; }

        public ConcurrentDictionary<string, float> FloatAttributes = new ConcurrentDictionary<string, float>();
        public ConcurrentDictionary<string, bool> BoolAttributes = new ConcurrentDictionary<string, bool>();
        public ConcurrentDictionary<string, int> IntAttributes = new ConcurrentDictionary<string, int>();
        public ConcurrentDictionary<string, string> StringAttributes = new ConcurrentDictionary<string, string>();
        public ConcurrentDictionary<string, Vec3> Vec3Attributes = new ConcurrentDictionary<string, Vec3>();
        public ConcurrentDictionary<string, Vec4> Vec4Attributes = new ConcurrentDictionary<string, Vec4>();
        public ConcurrentDictionary<string, Matrix3> Mat3Attributes = new ConcurrentDictionary<string, Matrix3>();

        private ConcurrentDictionary<string, FileObject> fileAttributes = new ConcurrentDictionary<string, FileObject>();
        private List<Attribute<UInt32>> fileHandles;

        private List<AvatarInstance> boardedInstances = null;   //list of all avatar instacnes that are currently in this structure
        private int maxBoard = 0;   //maximum number of avatar instances that can enter this structure

        public Descriptor Description { get; private set; }
        public Contract ActiveContract { get; private set; }
        public RealtimeControl Realtime { get; private set; }

        private static SortedList<string, int> MakeMap<T>(ConcurrentDictionary<string, T> dict)
        {
            SortedList<string, int> result = new SortedList<string, int>();

            int counter = 0;
            foreach (var pair in dict)
                result[pair.Key] = counter++;

            return result;
        }

        private Contract MakeContractFromThis()
        {
            lock (this)
            {
                Contract contract = new Contract();
                contract.Body.Type = Type;
                contract.Body.FloatAttributes = MakeMap(FloatAttributes);
                contract.Body.BoolAttributes = MakeMap(BoolAttributes);
                contract.Body.IntAttributes = MakeMap(IntAttributes);
                contract.Body.StringAttributes = MakeMap(StringAttributes);
                contract.Body.Vec3Attributes = MakeMap(Vec3Attributes);
                contract.Body.Vec4Attributes = MakeMap(Vec4Attributes);
                contract.Body.Mat3Attributes = MakeMap(Mat3Attributes);
                contract.Body.FileAttributes = MakeMap(fileAttributes);

                return contract;
            }
        }

        public void UpdateDescription()
        {
            lock (this)
            {
               // if (ActiveContract == null)//always check contract conformity
                {
                    lock (contracts)
                    {
                        Contract contract = MakeContractFromThis();
                        Contract existing;
                        if (contracts.TryGetValue(contract.Body, out existing))
                            ActiveContract = existing;
                        else
                        {
                            //foreach (var pair in contracts)
                            //{
                            //    if (pair.Key.GetHashCode() == contract.Body.GetHashCode())
                            //    {
                            //        Program.Log(null, Significance.Important, "Found matching type '" + pair.Key.Type);
                            //        if (!pair.Key.Equals(contract.Body))
                            //        {
                            //            Program.Log(null, Significance.Important, "Not equal");
                            //        }
                            //    }
                            //    else
                            //        Program.Log(null, Significance.Low, "Not equal '" + pair.Key.Type + "' (" + pair.Key.GetHashCode() + ")");
                            //}



                            lock (Contract.CodeLock)
                            {
                                contract.Code = Contract.CodeCounter++;
                            }
                            contract.UpdateBakedVersion();
                            ActiveContract = contract;
                            contracts[contract.Body] = contract;
                            Program.Log(null, Significance.Low, "Created new contract #" + contract.Code + " for type '" + contract.Body.Type + "' <"+contract.Body.GetHashCode().ToString("X")+">. Now having "+contracts.Count+" contracts registered");
                            //contract.Body.PrintToLog();
                        }
                    }
                }

                Descriptor desc = new Descriptor();


                desc.ID = ID.Value;
                desc.ContractCode = ActiveContract.Code;
                desc.BoundingVolume = BoundingVolume;
                desc.BoolAttributes = Bake(BoolAttributes, ActiveContract.Body.BoolAttributes);
                desc.IntAttributes = Bake(IntAttributes, ActiveContract.Body.IntAttributes);
                desc.FloatAttributes = Bake(FloatAttributes, ActiveContract.Body.FloatAttributes);
                desc.StringAttributes = Bake(StringAttributes, ActiveContract.Body.StringAttributes);
                desc.Vec3Attributes = Bake(Vec3Attributes, ActiveContract.Body.Vec3Attributes);
                desc.Vec4Attributes = Bake(Vec4Attributes, ActiveContract.Body.Vec4Attributes);
                desc.Mat3Attributes = Bake(Mat3Attributes, ActiveContract.Body.Mat3Attributes);
                desc.FileAttributes = GetFileHandles(ActiveContract.Body.FileAttributes);

                Description = desc;
            }
        }




        public void SetFileAttribute(string name, FileObject obj)
        {
            lock (fileAttributes)
            {
                FileObject current;
                if (fileAttributes.TryGetValue(name, out current))
                {
                    if (current == obj)
                        return;
                    if (current != null)
                        current.DecrementReferenceCount();
                }
                if (obj != null)
                    obj.IncrementReferenceCount();
                fileAttributes[name] = obj;
            }
        }


        public UInt32[] GetFileHandles(SortedList<string, int> map)
        {
            lock (fileAttributes)
            {
                if (map.Count != fileAttributes.Count)
                    throw new Contract.Violation("Attribute count mismatch. Expected " + map.Count + " elements in file attribute field (according to contract), but found " + fileAttributes.Count + " elements");
                UInt32[] result = new UInt32[fileAttributes.Count];
                foreach (var pair in fileAttributes)
                {
                    int address;
                    if (!map.TryGetValue(pair.Key, out address))
                        throw new Contract.Violation("Unable to find key '" + pair.Key + "' in map for attribute type file");
                    if (address < 0 || address >= result.Length)
                    {
                        Program.ReportFatal(null, new Exception("Internal consistency violation"));
                        return null;
                    }

                    result[address] = pair.Value.ID.Value;
                }
                return result;
            }
        }
        private static T[] Bake<T>(ConcurrentDictionary<string, T> dict, SortedList<string, int> map)
        {
            if (map.Count != dict.Count)
                throw new Contract.Violation("Attribute count mismatch. Expected " + map.Count + " elements in " + typeof(T).Name + " attribute field (according to contract), but found " + dict.Count + " elements");

            //KeyValuePair<string,T>[] attribs = dict.ToArray();
            T[] result = new T[dict.Count];
            foreach (var pair in dict)
            {
                int address;
                if (!map.TryGetValue(pair.Key,out address))
                    throw new Contract.Violation("Unable to find key '"+pair.Key+"' in map for attribute type "+typeof(T).Name);
                if (address < 0 || address >= result.Length)
                {
                    Program.ReportFatal(null,new Exception("Internal consistency violation"));
                    return null;
                }
                result[address] = pair.Value;
            }
            return result;
        }


        public bool ExportToFile { get; private set; }

        private void SetupTypeConstraints(ByteBuffer buffer)
        {
            lock (this)
            {
                maxBoard = 0;

                if (Type == "Ship")
                {
                    maxBoard = 1;
                    if (Realtime == null)
                        Realtime = new RealtimeControl(this);
                }
                else
                    Realtime = null;

                if (maxBoard == 0)
                {
                    boardedInstances = null;
                }
                else
                {
                    if (boardedInstances == null)
                        boardedInstances = new List<AvatarInstance>();
                    else
                    {
                        while (boardedInstances.Count > maxBoard)
                            boardedInstances[0].Board(null, buffer);
                    }
                }
            }
        }

        public Structure(string type, Box boundingVolume, bool shouldBeExported = false)
        {
            ExportToFile = shouldBeExported;
            Type = type;
            BoundingVolume = boundingVolume;

            SetupTypeConstraints(null);
        }


        public Structure(XElement xstructure)
        {
            fileHandles = new List<Attribute<uint>>();
            ExportToFile = true;
            Type = xstructure.GetString("type");
            BoundingVolume = new Box(xstructure.GetVec3("min"), xstructure.GetVec3("max"));



            IEnumerable<XElement> elements = xstructure.Elements();
            //foreach (XElement xgroup in elements)
            {
               // if (xgroup.Name == "dictionary")
                {
                    foreach (XElement xchild in elements)
                    {
                        try
                        {
                            string name = xchild.GetString("name");
                            if (name == "realtime")
                            {
                                Realtime = new RealtimeControl(this);

                                Realtime.Captured = NetTime.Query();
                                Realtime.System.x = new Vec4(xchild.GetVec3("x"),0);
                                Realtime.System.y = new Vec4(xchild.GetVec3("y"), 0);
                                Realtime.System.z = new Vec4(xchild.GetVec3("z"), 0);
                                Realtime.System.w = new Vec4(xchild.GetVec3("w"), 1);
                                Realtime.LinearVelocity = xchild.GetVec3("velocity");
                                Realtime.Momentum = xchild.GetVec3("momentum");
                            }
                            else if (xchild.Name == "int")
                                IntAttributes[name] = xchild.GetInt32("value");
                            else if (xchild.Name == "float")
                                FloatAttributes[name] = xchild.GetFloat("value");
                            else if (xchild.Name == "bool")
                                BoolAttributes[name] = xchild.GetBool("value");
                            else if (xchild.Name == "string")
                                StringAttributes[name] = xchild.GetString("value");
                            else if (xchild.Name == "vec3")
                                Vec3Attributes[name] = xchild.GetVec3("value");
                            else if (xchild.Name == "vec4")
                                Vec4Attributes[name] = xchild.GetVec4("value");
                            else if (xchild.Name == "mat3")
                                Mat3Attributes[name] = new Matrix3(
                                                        xchild.GetVec3("x"),
                                                        xchild.GetVec3("y"),
                                                        xchild.GetVec3("z")
                                                        );
                            else if (xchild.Name == "file")
                                fileHandles.Add(new Attribute<uint>(name, xchild.GetUInt32("id")));
                            else
                                throw new Exception("Unexpected entry type '" + xchild.Name + "' encountered while parsing structure");
                        }
                        catch (Exception e)
                        {
                            Program.ReportUnexpected(null, e, Significance.Unusual);
                        }
                    }
                }
                //else if (xgroup.Name == "list")
                //{
                //}
            }
            SetupTypeConstraints(null);
        }

        public static void Load(XElement xstructure)
        {
            Structure s = new Structure(xstructure);
            Collection.InsertExisting(xstructure.GetUInt32("id"), s);
            if (s.Realtime != null)
                RealtimeStructures.Insert(s);
            else
                BVH.Insert(s, s.BoundingVolume);
            s.UpdateDescription();
        }

        private static void InternalSet<T>(ConcurrentDictionary<string, T> dict, string[] names, T[] values)
        {
            if (names == null || values == null)
                return;
            Debug.Assert(names.Length == values.Length);
            for (int i = 0; i < names.Length; i++)
                dict[names[i]] = values[i];
        }
        private static void InternalSetFiles(Structure s, string[] names, UInt32[] handles)
        {
            if (names == null || handles == null)
                return;
            Debug.Assert(names.Length == handles.Length);
            for (int i = 0; i < names.Length; i++)
                s.SetFileAttribute(names[i], FileObject.Collection.Lookup(handles[i]));
        }

        public static Structure Load(Contract.Baked contract, Descriptor descriptor)
        {
            Structure s = new Structure(contract.TypeName, descriptor.BoundingVolume, true);
            InternalSet(s.Vec3Attributes, contract.Vec3Attributes, descriptor.Vec3Attributes);
            InternalSet(s.Vec4Attributes, contract.Vec4Attributes, descriptor.Vec4Attributes);
            InternalSet(s.FloatAttributes, contract.FloatAttributes, descriptor.FloatAttributes);
            InternalSet(s.BoolAttributes, contract.BoolAttributes, descriptor.BoolAttributes);
            InternalSet(s.IntAttributes, contract.IntAttributes, descriptor.IntAttributes);
            InternalSet(s.StringAttributes, contract.StringAttributes, descriptor.StringAttributes);
            InternalSetFiles(s,contract.FileAttributes, descriptor.FileAttributes);
            s.UpdateDescription();
            return s;
        }



        public virtual void PostLoadLink()
        {
            if (fileHandles != null)
                foreach (var entry in fileHandles)
                {
                    FileObject obj = FileObject.Collection.Lookup(entry.Value);
                    if (obj == null)
                        throw new Exception("No file found for handle '" + entry.Value + "' of file attribute '" + entry.Name + "'");
                    SetFileAttribute(entry.Name, obj);
                }
            fileHandles = null;
        }

        public virtual XElement ToXML()
        {
            if (!ExportToFile)
                throw new SkippingElementException();

            XElement xresult = new XElement("structure");
            lock (this)
            {
                xresult.Set("id", ID);
                xresult.Set("type", Type);
                xresult.Set("min", BoundingVolume.Min);
                xresult.Set("max", BoundingVolume.Max);
                //XElement xdictionary = new XElement("dictionary");

                if (Realtime != null)
                {
                    XElement xel = new XElement("realtime");

                    xel.Set("x", Realtime.System.x.xyz);
                    xel.Set("y", Realtime.System.y.xyz);
                    xel.Set("z", Realtime.System.z.xyz);
                    xel.Set("w", Realtime.System.w.xyz);
                    xel.Set("velocity", Realtime.LinearVelocity);
                    xel.Set("momentum", Realtime.Momentum);

                    xresult.Add(xel);
                }


                foreach (var pair in IntAttributes)
                {
                    XElement xel = new XElement("int");
                    xel.Set("name", pair.Key);
                    xel.Set("value", pair.Value);
                    xresult.Add(xel);
                }
                foreach (var pair in FloatAttributes)
                {
                    XElement xel = new XElement("float");
                    xel.Set("name", pair.Key);
                    xel.Set("value", pair.Value);
                    xresult.Add(xel);
                }
                foreach (var pair in BoolAttributes)
                {
                    XElement xel = new XElement("bool");
                    xel.Set("name", pair.Key);
                    xel.Set("value", pair.Value);
                    xresult.Add(xel);
                }
                foreach (var pair in StringAttributes)
                {
                    XElement xel = new XElement("string");
                    xel.Set("name", pair.Key);
                    xel.Set("value", pair.Value);
                    xresult.Add(xel);
                }
                foreach (var pair in Vec3Attributes)
                {
                    XElement xel = new XElement("vec3");
                    xel.Set("name", pair.Key);
                    xel.Set("value", pair.Value);
                    xresult.Add(xel);
                }
                foreach (var pair in Vec4Attributes)
                {
                    XElement xel = new XElement("vec4");
                    xel.Set("name", pair.Key);
                    xel.Set("value", pair.Value);
                    xresult.Add(xel);
                }
                foreach (var pair in Mat3Attributes)
                {
                    XElement xel = new XElement("mat3");
                    xel.Set("name", pair.Key);
                    xel.Set("x", pair.Value.x);
                    xel.Set("y", pair.Value.y);
                    xel.Set("z", pair.Value.z);
                    xresult.Add(xel);
                }
                lock (fileAttributes)
                    foreach (var pair in fileAttributes)
                    {
                        XElement xel = new XElement("file");
                        xel.Set("name", pair.Key);
                        xel.Set("id", pair.Value.ID.Value);
                        xresult.Add(xel);
                    }
               // xresult.Add(xdictionary);

                //XElement xlist = new XElement("list");

                //xresult.Add(xlist);

                return xresult;
            }
        }



        public override string ToString()
        {
            return ("Structure." + Type + "(" + ID + ")");
        }

        public void Destroy()
        {
            lock (fileAttributes)
            {
                foreach (var pair in fileAttributes)
                {
                    pair.Value.DecrementReferenceCount();
                }
                fileAttributes.Clear();
            }
        }

        public void Dispose()
        {
            Destroy();
            Collection.Remove(this);
            if (Realtime != null)
                RealtimeStructures.Remove(this);
            else
                BVH.Remove(this);

        }


        internal void Eject(AvatarInstance avatarInstance)
        {
            lock (this)
            {
                boardedInstances.Remove(avatarInstance);
            }
        }
        internal bool Include(AvatarInstance avatarInstance)
        {
            lock (this)
            {
                if (boardedInstances == null || maxBoard <= boardedInstances.Count)
                    return false;
                boardedInstances.Add(avatarInstance);
                return true;
            }
        }


        /// <summary>
        /// Checks whether or not the local structure is occupied/boarded by at least one avatar instance
        /// </summary>
        public bool IsOccupied
        {
            get
            {
                lock (this)
                {
                    return boardedInstances != null && boardedInstances.Count > 0;
                }
            }
        }

    }
}
