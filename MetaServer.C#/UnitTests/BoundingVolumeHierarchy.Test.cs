using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using System.Diagnostics;


using Consensus;
using Element = Math3D.BoundingVolumeHierarchy<int>.Entry;

namespace UnitTests
{
    [TestClass]
    public class BoundingVolumeHierarchy
    {

        private static Random random = new Random();

        private static Math3D.Box randomBox()
        {
            float centerX = (float)random.NextDouble() * 2.0f - 1.0f;
            float centerY = (float)random.NextDouble() * 2.0f - 1.0f;
            float centerZ = (float)random.NextDouble() * 2.0f - 1.0f;
            float extX = (float)random.NextDouble() * 0.25f + 0.1f;
            float extY = (float)random.NextDouble() * 0.25f + 0.1f;
            float extZ = (float)random.NextDouble() * 0.25f + 0.1f;
            return new Math3D.Box
            {
                x = new Math3D.Range(centerX - extX, centerX + extX),
                y = new Math3D.Range(centerY - extY, centerY + extY),
                z = new Math3D.Range(centerZ - extZ, centerZ + extZ)
            };
        }

        private static Element randomElement()
        {
            return new Element(random.Next(),randomBox());
        }

        [TestMethod]
        public void TestBVHCoverage()
        {

            for (int iteration = 0; iteration < 10; iteration++)
            {
                List<Element> elements = new List<Element>();
                for (int i = 0; i < (1<<(iteration+1)); i++)
                    elements.Add(randomElement());
                Math3D.BoundingVolumeHierarchy<int> tree = new Math3D.BoundingVolumeHierarchy<int>();
				LoadingStage.OnExit += () => tree.RebuildIfNeeded();

                tree.RuntimeInsert(elements);
                //tree.ForceReconstruction(true);

                int overcast = 0;
                int numtests = 0;

                List<int> lookupResult = new List<int>();
                foreach (Element el in elements)
                {
                    lookupResult.Clear();
                    int numChecks;

					Math3D.Vec3 sampleAt = new Math3D.Vec3(random.NextIn(el.BoundingBox.x), random.NextIn(el.BoundingBox.y), random.NextIn(el.BoundingBox.z));
                    Assert.IsTrue(el.BoundingBox.Contains(sampleAt));


                    tree.Lookup(sampleAt, ref lookupResult, out numChecks );
                    //tree.Lookup(el.Box, ref lookupResult, out numChecks);
                    numtests += numChecks;
                    overcast += (lookupResult.Count - 1);
                    if (!lookupResult.Contains(el.Element))
                    {
                        Debug.WriteLine("Lookup failed of element " + el.Element + " in " + el.BoundingBox + " at " + sampleAt + ". Tracing lookup...");
                        tree.TraceLookup(sampleAt);
                        Assert.Fail("Lookup failed");
                    }
                }
                Debug.WriteLine("Volume tests at iteration "+iteration+": " + (float)numtests / elements.Count+ " tests per entry with a total of " + elements.Count +" entries");
            }


        }
    }
}
