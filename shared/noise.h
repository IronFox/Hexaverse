#ifndef noiseH
#define noiseH

#include <math/vector.h>

//#include "ExtRenderer/DirectX11.h"

namespace PerlinNoisePermutation
{
	extern BYTE permutation[];
}


float N(float x, float y, float z);
float NSym(float x, float y, float z);

class BasePerlinNoise
{
public:
	BYTE p[512];
	//Engine::DX11::Texture noiseMap;

	BasePerlinNoise();
	void Create();
};

template <typename T>
	class PerlinNoise : public BasePerlinNoise
	{
	public:
	   T Get(T x, T y, T z)	const
	   {
		   int	X = (int)std::floor(x) & 255,                  // FIND UNIT CUBE THAT
				Y = (int)std::floor(y) & 255,                  // CONTAINS POINT.
				Z = (int)std::floor(z) & 255;
			x -= std::floor(x);                                // FIND RELATIVE X,Y,Z
			y -= std::floor(y);                                // OF POINT IN CUBE.
			z -= std::floor(z);
			T	u = fade(x),                                // COMPUTE FADE CURVES
				v = fade(y),                                // FOR EACH OF X,Y,Z.
				w = fade(z);
			int A = p[X  ]+Y, AA = p[A]+Z, AB = p[A+1]+Z,      // HASH COORDINATES OF
				B = p[X+1]+Y, BA = p[B]+Z, BB = p[B+1]+Z;      // THE 8 CUBE CORNERS,
			return lerp(w, lerp(v, lerp(u, grad(p[AA  ], x  , y  , z   ),  // AND ADD
											grad(p[BA  ], x-1, y  , z   )), // BLENDED
									 lerp(u, grad(p[AB  ], x  , y-1, z   ),  // RESULTS
											 grad(p[BB  ], x-1, y-1, z   ))),// FROM  8
							 lerp(v, lerp(u, grad(p[AA+1], x  , y  , z-1 ),  // CORNERS
											 grad(p[BA+1], x-1, y  , z-1 )), // OF CUBE
									 lerp(u, grad(p[AB+1], x  , y-1, z-1 ),
											 grad(p[BB+1], x-1, y-1, z-1 ))));
	   }
	   static T fade(T t) { return t * t * t * (t * (t * 6 - 15) + 10); }
	   static T lerp(T t, T a, T b) { return a + t * (b - a); }
	   static T grad(int hash, T x, T y, T z)
	   {
		  int h = hash & 15;                      // CONVERT LO 4 BITS OF HASH CODE
		  T u = h<8 ? x : y,                 // INTO 12 GRADIENT DIRECTIONS.
			v = h<4 ? y : h==12||h==14 ? x : z;
		  return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
	   }

		T Get(const TVec3<T>&xyz)	const
		{
			return Get(xyz.x,xyz.y,xyz.z);
		}
		T Get(const TVec3<T>&xyz, float frequency)	const
		{
			return Get(xyz.x*frequency,xyz.y*frequency,xyz.z*frequency);
		}


	};


extern PerlinNoise<float>	noise;

#endif
