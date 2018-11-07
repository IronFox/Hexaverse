#ifndef surfaceRendererH
#define surfaceRendererH
#ifdef HAS_WINDOW

#include "common.h"
#include <math/vector.h>


namespace SurfaceRenderer
{
	using namespace DeltaWorks;

	namespace DX11 = Engine::DX11;

	void	Create();
	void	Destroy();

	void	SetColor(const float4&color);
	void	Rasterize(const M::Rect<>&);
	void	RasterizeConvexPolygon(const M::TVec2<>*field, count_t numVertices);
	void	Rasterize(const M::TVec2<>&p0, const M::TVec2<>&p1, const M::TVec2<>&p2);
	void	Rasterize(const M::TVec2<>&p0, const M::TVec2<>&p1, const M::TVec2<>&p2, const M::TVec2<>&p3);
	void	Rasterize(const M::TVec2<>&p0, const M::TVec4<>&c0, const M::TVec2<>&p1, const M::TVec4<>&c1, const M::TVec2<>&p2, const M::TVec4<>&c2);
	void	Rasterize(const M::TVec2<>&p0, const M::TVec4<>&c0, const M::TVec2<>&p1, const M::TVec4<>&c1, const M::TVec2<>&p2, const M::TVec4<>&c2, const M::TVec2<>&p3, const M::TVec4<>&c3);
	//void	Rasterize(const float3&p0, const float4&c0, const float3&p1, const float4&c1);
	void	Flush();


	class Baked
	{
	public:
		struct TVertex
		{
			float2	position;
			float4	color;
		};

		void		AddVertex(const float2&p, const float4&color = float4(1)) {TVertex&v = AddVertex(); v.position = p; v.color = color;}
		void		Add(const TVertex&vtx) {vertices.Append(vtx);}
		TVertex&	AddVertex() {return vertices.Append();}
		void		AddTriangle(index_t v0, index_t v1, index_t v2)
		{
			indices << UINT32(v0 + indexOffset) << UINT32(v1 + indexOffset) << UINT32(v2 + indexOffset);
		}
		void		AddTriangleInv(index_t v0, index_t v1, index_t v2)
		{
			indices << UINT32(v0 + indexOffset) << UINT32(v2 + indexOffset) << UINT32(v1 + indexOffset);
		}
		void		AddQuad(index_t v0, index_t v1, index_t v2, index_t v3)
		{
			AddTriangle(v0,v1,v2);
			AddTriangle(v0,v2,v3);
		}
		void		AddQuadInv(index_t v0, index_t v1, index_t v2, index_t v3)
		{
			AddTriangleInv(v0,v1,v2);
			AddTriangleInv(v0,v2,v3);
		}


		void	SetIndexOffsetToVertexCount()
		{
			indexOffset = vertices.Count();
		}

		void	Clear()	{vertices.Clear(); indices.Clear(); indexOffset = 0;}
		void	Destroy()	{Clear(); instruction.Clear();}

		void	Bake();
		void	Rasterize(const float4&tint)	const;

		friend void	swap(Baked&a, Baked&b)
		{
			using std::swap;
			swap(a.instruction,b.instruction);
			swap(a.vertices,b.vertices);
			swap(a.indices,b.indices);
			swap(a.indexOffset,b.indexOffset);
		}

	private:
		DX11::InstructionBlock32	instruction;

		Buffer0<TVertex>			vertices;
		Buffer0<UINT32>				indices;
		count_t						indexOffset=0;
	};
}

#endif
#endif
