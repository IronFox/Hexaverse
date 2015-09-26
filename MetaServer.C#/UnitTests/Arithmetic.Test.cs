using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;

using Consensus;
using System.Diagnostics;

namespace UnitTests
{
	using M = Math3D;

	[TestClass]
    public class Arithmetic
    {
        [TestMethod]
        public void TestCrossProduct()
        {
			{
				M.Range r = new M.Range(-100, 100);
				Random random = new Random();
				const float threshold = 0.00001f;
				for (int i = 0; i < 10000; i++)
				{
					M.Vec3 u = new M.Vec3(random.NextIn(r), random.NextIn(r), random.NextIn(r)).Normalize();
					M.Vec3 v = new M.Vec3(random.NextIn(r), random.NextIn(r), random.NextIn(r)).Normalize();

					if (u.Length > 0 && v.Length > 0 && Math.Abs(M.Vec.Dot(u, v)) < u.Length * v.Length)
					{
						M.Vec3 crossed = M.Vec.Cross(u, v).Normalize();
						float error = Math.Abs(M.Vec.Dot(crossed, u));
						if (error >= threshold)
						{
							Debug.WriteLine(u + " x " + v + " = " + crossed + ", error u = " + error);
							Assert.Fail(u + " x " + v + " = " + crossed + ", error u = " + error);
						}
						error = Math.Abs(M.Vec.Dot(crossed, v));
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
}
