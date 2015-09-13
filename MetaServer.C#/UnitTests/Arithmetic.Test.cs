using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;

using Consensus;
using System.Diagnostics;

namespace UnitTests
{
    [TestClass]
    public class Arithmetic
    {
        [TestMethod]
        public void TestCrossProduct()
        {
            Range r = new Range(-100, 100);
            Random random = new Random();
            const float threshold = 0.00001f;
            for (int i = 0; i < 10000; i++)
            {
                Vec3 u = new Vec3(random.NextIn(r), random.NextIn(r), random.NextIn(r)).Normalize();
                Vec3 v = new Vec3(random.NextIn(r), random.NextIn(r), random.NextIn(r)).Normalize();

                if (u.Length > 0 && v.Length > 0 && Math.Abs(Vec.Dot(u, v)) < u.Length * v.Length)
                {
                    Vec3 crossed = Vec.Cross(u, v).Normalize();
                    float error = Math.Abs(Vec.Dot(crossed, u));
                    if (error >= threshold)
                    {
                        Debug.WriteLine(u + " x " + v + " = " + crossed + ", error u = " + error);
                        Assert.Fail(u + " x " + v + " = " + crossed + ", error u = " + error);
                    }
                    error = Math.Abs(Vec.Dot(crossed, v));
                    if (error >= threshold)
                    {
                        Debug.WriteLine(u + " x " + v + " = " + crossed + ", error v = " + error);
                        Assert.Fail(u + " x " + v + " = " + crossed + ", error v = " + error);
                    }
                }

            }
        }
    }
}
