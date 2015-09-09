using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Xml.Linq;

namespace Consensus
{

    public class AvatarInstance : Entity
    {
        public enum Flags
        {
            IsOnline = 0x1,
            IsIdle = 0x2,
            IsAFK = 0x4,
            IsBusy = 0x8,
            IsInConversation = 0x10
        }
        public Matrix4 System {get; private set;}
        public Vec3 LinearVelocity;

        public Account Owner { get; private set; }
        
        private UInt32 ownerID = ID.Invalid.Value;
        public Client ControlledBy = null;
        private UInt32 boardedStructureID = ID.Invalid.Value;

        private volatile int versionCounter = 0;
        public long LastCaptured;

        public static IDMap<AvatarInstance> Collection = new IDMap<AvatarInstance>();
        public List<Structure> CurrentContexts = new List<Structure>();

        public Vec3 ActualPosition { get { return System.Position + LinearVelocity * NetTime.ToSecondsf(NetTime.Query() - LastCaptured); } }


        public AvatarInstance()
        { 
            System = new Matrix4(1);
        }

        public AvatarInstance(Account owner, Matrix4 system)
        {
            Owner = owner;
            SetSystem(system);
        }

        public void SetSystem(Matrix4 m)
        {
            Program.DebugCheckGoodSystem(m);
            System = m;
        }

        public void SignalStateChange()
        {
            lock (this)
                versionCounter++;
        }
        public UInt32 GetFlags()
        {
            Flags result = 0;
            if (ControlledBy != null)
                result |= Flags.IsOnline;

            return (UInt32)result;
        }
        public void PostLoadLink()
        {
            Owner = Account.Collection.Lookup(ownerID);
            Debug.Assert(Owner != null);
            Owner.Avatars.Add(this);
            
            if (boardedStructureID != ID.Invalid)
                Board(Structure.Collection.Lookup(boardedStructureID),null);

            Structure.BVH.Lookup(System.Position, ref CurrentContexts);
            Program.Log(null, Significance.Low, "Set contexts of " + this + " to:");
            foreach (var structure in CurrentContexts)
                Program.Log(null, Significance.Low, "  " + structure);
        }

        public static void Load(XElement xinstance)
        {
            UInt32 idValue = xinstance.GetUInt32("id");
            UInt32 accID = xinstance.GetUInt32("account_id");
            Vec3 pos = new Vec3(xinstance.GetFloat("x"),
                                xinstance.GetFloat("y"),
                                xinstance.GetFloat("z"));
            Vec3 dir = new Vec3(xinstance.GetFloat("dir_x"),
                                xinstance.GetFloat("dir_y"),
                                xinstance.GetFloat("dir_z"));
                
            AvatarInstance instance = new AvatarInstance();

            uint dist;
            if (xinstance.TryGetUInt32("third_person_distance",out dist))
                instance.ThirdPersonDistance = (byte)dist;
            else
                instance.ThirdPersonDistance = 0;
            if (!xinstance.TryGetUInt32("boarded_structure", out instance.boardedStructureID))
                instance.boardedStructureID = ID.Invalid.Value;
            instance.ownerID = accID;
            Matrix4 sys = new Matrix4(1);
            sys.w.xyz = pos;
            sys.y.xyz = dir.Normalize();
            sys.x.xyz = Vec.Cross(dir,new Vec3(0, 0, 1)).Normalize();
            sys.z.xyz = Vec.Cross(sys.x.xyz, sys.y.xyz).Normalize();
            Debug.Assert(sys.z.z > 0.0f);
            instance.SetSystem(sys);
            instance.LastCaptured = NetTime.Query();
            //account.Avatars.Add(av);
            Collection.InsertExisting(idValue, instance);
        }
        public XElement ToXML()
        {
            lock (this)
            {
                XElement xavatar = new XElement("avatar");
                xavatar.Set("id", ID);
                xavatar.Set("account_id", Owner.ID);
                xavatar.Set("x", System.w.x);
                xavatar.Set("y", System.w.y);
                xavatar.Set("z", System.w.z);
                xavatar.Set("dir_x", System.y.x);
                xavatar.Set("dir_y", System.y.y);
                xavatar.Set("dir_z", System.y.z);
                xavatar.Set("third_person_distance", (uint)ThirdPersonDistance);
                if (BoardedStructure != null)
                    xavatar.Set("boarded_structure", BoardedStructure.ID);
                return xavatar;
            }
        }
        public bool Update(Matrix4 system, Vec3 linearVelocity, byte thirdPersonDistance, long timeStamp)
        {
            lock (this)
            {
                LastCaptured = timeStamp;

                if (system == System && linearVelocity == LinearVelocity && thirdPersonDistance == ThirdPersonDistance)
                    return true;

                Program.DebugCheckGoodSystem(system);
                //Debug.Assert(age >= 0 && Time.ToSeconds(age) < 10.0f);
                System = system;
                LinearVelocity = linearVelocity;
                versionCounter++;
                ThirdPersonDistance = thirdPersonDistance;

                double delta = NetTime.ToSeconds(NetTime.Query() - timeStamp);
                if (delta > 10.0)
                    Program.Log(null, Significance.Unusual, "round-trip time exceeded 10 seconds: " + delta);

                return delta < 10.0;
            }
        }
        public override string ToString()
        {
            return "Avatar('" + Owner.Name + "'/"+Owner.ID+"/" + ID + ")";
        }
        public bool IsOutdated(int version)
        {
            lock (this)
                return versionCounter != version;
        }

        public int GetVersion()
        {
            lock (this)
                return versionCounter;
        }

        public void Deactivate()
        {
            lock (this)
            {
                LinearVelocity = Vec3.Zero;
                versionCounter++;
                ControlledBy = null;
            }
        }

        internal uint[] BakeCurrentContextIDs()
        {
            lock (CurrentContexts)
            {
                uint[] result = new UInt32[CurrentContexts.Count];
                for (int i = 0; i < result.Length; i++)
                    result[i] = CurrentContexts[i].ID.Value;
                return result;
            }
        }

        public bool Board(Structure strct, ByteBuffer buffer)
        {
            lock (this)
            {
                if (BoardedStructure == strct)
                    return true;
                if (BoardedStructure != null)
                    BoardedStructure.Eject(this);
                BoardedStructure = strct;
                bool success = true;
                if (strct != null)
                {
                    success = strct.Include(this);
                    if (!success)
                        BoardedStructure = null;
                }
                if (buffer != null)
                    Interface.Out.Channel.AvatarConditionChange.Broadcast(buffer, new Interface.Out.Struct.AvatarConditionChange(this));
                return success;
            }
        }


        public byte ThirdPersonDistance { get; private set; }

        public Structure BoardedStructure { get; private set; }
    }

}
