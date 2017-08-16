#ifndef surfaceRendererH
#define surfaceRendererH
#ifdef HAS_WINDOW

#include "../../ExtRenderer/DirectX11.h"
#include <math/vector.h>


namespace SurfaceRenderer
{
	using namespace DeltaWorks;
	using namespace DeltaWorks::Math;

	namespace DX11 = Engine::DX11;

	void	Create();
	void	Destroy();

	void	SetColor(const float4&color);
	void	Rasterize(const Rect<>&);
	void	RasterizeConvexPolygon(const TVec2<>*field, count_t numVertices);
	void	Rasterize(const TVec2<>&p0, const TVec2<>&p1, const TVec2<>&p2);
	void	Rasterize(const TVec2<>&p0, const TVec2<>&p1, const TVec2<>&p2, const TVec2<>&p3);
	void	Rasterize(const TVec2<>&p0, const TVec4<>&c0, const TVec2<>&p1, const TVec4<>&c1, const TVec2<>&p2, const TVec4<>&c2);
	void	Rasterize(const TVec2<>&p0, const TVec4<>&c0, const TVec2<>&p1, const TVec4<>&c1, const TVec2<>&p2, const TVec4<>&c2, const TVec2<>&p3, const TVec4<>&c3);
	//void	Rasterize(const float3&p0, const float4&c0, const float3&p1, const float4&c1);
	void	Flush();


	class Baked
	{
	public:
		struct TVertex
		{
			M::float2	position;
			M::float4	color;
		};

		void		AddVertex(const M::float2&p, const M::float4&color = M::float4(1)) {TVertex&v = AddVertex(); v.position = p; v.color = color;}
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
		void	Rasterize()	const;

	private:
		DX11::InstructionBlock32	instruction;

		Buffer0<TVertex>			vertices;
		Buffer0<UINT32>				indices;
		count_t						indexOffset=0;
	};
}

#endif
#endif
