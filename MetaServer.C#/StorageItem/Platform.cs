using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Xml.Linq;
//using Prisma.StorageItem;

namespace Consensus
{
    public class PrimaryPlatform : Structure
    {
        private float _radius=1.0f;
        private Vec3    _position;
        public float Radius { get {return _radius;} set{base.FloatAttributes["Radius"] = value; _radius = value; UpdateSpace();} }
        public Vec3 Position {get {return _position;} set{base.Vec3Attributes["Position"] = value; _position = value; UpdateSpace();} }
        private FileObject _icon;
        public FileObject Icon 
        {
            get
            {
                return _icon; 
            }
            set 
            { 
                if (_icon == value)
                    return;
                _icon = value;
                base.SetFileAttribute("Icon",value);
                UpdateDescription();
            }
        }
        private UInt32 iconID = ID.Invalid.Value;



        private static object _lock = new object();
        private static PrimaryPlatform _singleton;
        public static PrimaryPlatform Singleton { 
            get {
                lock (_lock)
                    if (_singleton == null)
                    {
                        _singleton = new PrimaryPlatform(10.0f, new Vec3(0, 0, 50));
                    }
                return _singleton;
            }
        }

        public static PrimaryPlatform SpawnDebugPlatform(Vec3 position, float radius)
        {
            #if DEBUG
                return new PrimaryPlatform(radius, position);
            #else
                return null;
            #endif
        }

//        public static Platform Primary = new Platform(10.0f, new Vec3(0));

        public static Box GetBoundingVolume(Vec3 position, float radius)
        {
            return new Box(
                        new Vec3(position.xy - radius - 2.0f, position.z - 1.0f), 
                        new Vec3(position.xy + radius + 2.0f, position.z + 10.0f)
                        );

        }

        private void UpdateSpace()
        {
            BoundingVolume = GetBoundingVolume(_position, _radius);
        }

        private PrimaryPlatform(float radius, Vec3 position)
            : base("Platform", GetBoundingVolume(position, radius))
        {
            Radius = radius;
            Position = position;
            UpdateDescription();
            //UpdateSpace();
        }

        public override XElement ToXML()
        {
            throw new SkippingElementException();
        }



        public Matrix4 SpawnAvatarPosition()
        {
            Random  random = new Random();
            Vec3 pos = Position;
            double angle = random.NextDouble() * 2.0 * Math.PI;
            double radius = random.NextDouble() * Radius * 0.9;
            pos.x += (float)(Math.Sin(angle) * radius);
            pos.y += (float)(Math.Cos(angle) * radius);
            pos.z += 2.0f;

            double lookAngle = random.NextDouble() * 2.0 * Math.PI;

            Matrix4 result = Matrix4.Identity;
            result.x.x = (float)Math.Cos(lookAngle);
            result.x.y = (float)Math.Sin(lookAngle);
            result.y.x = (float)-Math.Sin(lookAngle);
            result.y.y = (float)Math.Cos(lookAngle);
            result.Position = pos;
            return result;
        }




        internal static void DestroySingleton()
        {
            lock (_lock)
            {
                if (_singleton != null)
                    _singleton.Dispose();
                _singleton = null;
            }
        }

    }
}
