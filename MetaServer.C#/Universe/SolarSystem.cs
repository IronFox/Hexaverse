using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Math3D;

namespace Consensus.Universe
{
	public class SolarSystem
	{
		Sun sun;
		List<OrbitingBody> bodies;
		public Account Owner { get; set; }

		public void Build(Random random)
		{
			sun = new Sun(Guid.NewGuid());
			bodies = new List<OrbitingBody>();

			int numPlanets = random.Next(1, 11);
			float r0 = random.NextIn(new Range(4e10f, 8e10f));
			float r1 = random.NextIn(new Range(1e12f, 8e12f));
			Range planetRange = new Range(r0, r1 - r0);

			float[] w = new float[numPlanets];
			float total = 1;
			for (int i = 0; i < numPlanets; i++)
			{
				float r = random.NextFloat() + 0.25f;
				w[i] = r;
				total *= (1f + r);
			}

			float at = 1;
			for (int i = 0; i < numPlanets; i++)
			{
				float r = w[i];
				float at2 = at * (1f + r);
				float c = ((at + at2) / 2f - 1) / (total - 1);
				float radius = planetRange.At(c);

				bodies.Add(new OrbitingBody(PlanetStructure.GetRandom(random),  sun, radius, Guid.NewGuid(), random.NextFloat() * 2f * (float)Math.PI));

				at = at2;
			}
		}
	}
}
