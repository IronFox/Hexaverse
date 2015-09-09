using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Xml.Linq;

namespace Consensus
{


    public abstract class Surface : Entity
    {
        public Vec3 Position { get; protected set; }
        public Box Space { get; protected set; }

        protected Surface()
        {}

        protected Surface(XElement xground)
        {
            Position = new Vec3(xground.GetFloat("x"),
                                xground.GetFloat("y"),
                                xground.GetFloat("z"));
        }

        protected void ExportTo(XElement xground)
        {
            xground.Set("id", ID);
            xground.Set("x", Position.x);
            xground.Set("y", Position.y);
            xground.Set("z", Position.z);
        }

        public virtual void PostLoadLink() { }

        public override string ToString()
        {
            return ("Ground{ID=" + ID + "}");
        }


        public abstract XElement ToXML();
    }
}
