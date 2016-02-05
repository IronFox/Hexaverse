#include <global_root.h>
#include "geometry.h"
#include <math/object.h>
#include <geometry/cgs.h>
#include <io/file_system.h>
#include <geometry/converter/obj.h>
#include "../logEvent.h"

namespace Objects
{





	static void MakeBox2(Geometry&geometry, const Box<>&box, count_t ySegments, const float4&color = float4(1))
	{
		geometry.SetVertexOffsetToCurrent();
		for (index_t iy = 0; iy <= ySegments; iy++)
		{
			float fy = float(iy) / ySegments;
			float y = box.y.Derelativate(fy);
			geometry.MakeVertex(float3(box.x.min, y, box.z.max), float3(-1, 0, 0), color);
			geometry.MakeVertex(float3(box.x.min, y, box.z.min), float3(-1, 0, 0), color);
			if (iy < ySegments)
				geometry.MakeQuadInv(2 * iy, 2 * iy + 1, 2 * iy + 3, 2 * iy + 2);
		}

		geometry.SetVertexOffsetToCurrent();
		for (index_t iy = 0; iy <= ySegments; iy++)
		{
			float fy = float(iy) / ySegments;
			float y = box.y.Derelativate(fy);
			geometry.MakeVertex(float3(box.x.max, y, box.z.max), float3(1, 0, 0), color);
			geometry.MakeVertex(float3(box.x.max, y, box.z.min), float3(1, 0, 0), color);
			if (iy < ySegments)
				geometry.MakeQuad(2 * iy, 2 * iy + 1, 2 * iy + 3, 2 * iy + 2);
		}

		geometry.SetVertexOffsetToCurrent();
		for (index_t iy = 0; iy <= ySegments; iy++)
		{
			float fy = float(iy) / ySegments;
			float y = box.y.Derelativate(fy);
			geometry.MakeVertex(float3(box.x.min, y, box.z.max), float3(0, 0, 1), color);
			geometry.MakeVertex(float3(box.x.max, y, box.z.max), float3(0, 0, 1), color);
			if (iy < ySegments)
				geometry.MakeQuad(2 * iy, 2 * iy + 1, 2 * iy + 3, 2 * iy + 2);
		}



		//these stay the same
		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.min), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.min), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.max), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.max), float3(0, 1, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);



		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.min), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.min), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.max), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.max), float3(0, -1, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);

	}



	static void MakeBox3(Geometry&geometry, const Box<>&box, const float4&color = float4(1))
	{
		const float x0 = box.x.Derelativate(0.05f);
		const float x1 = box.x.Derelativate(0.95f);
		const float y0 = box.y.Derelativate(0.1f);
		const float y1 = box.y.Derelativate(0.9f);
		const float z0 = box.z.Derelativate(0.83f);


		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.min), float3(-1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.min), float3(-1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.min, z0), float3(-1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, z0), float3(-1, 0, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);

		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.min), float3(1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.min), float3(1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, z0), float3(1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, z0), float3(1, 0, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);


		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.min), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.min), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, z0), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, z0), float3(0, 1, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);


		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.min), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.min), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, z0), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.min, z0), float3(0, -1, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);

	
		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.min, z0), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, z0), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x1, y0, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x0, y0, box.z.max), float3(0, 0, 1), color);
		geometry.MakeQuad(0, 1, 2, 3);

		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.max, z0), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, z0), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x1, y1, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x0, y1, box.z.max), float3(0, 0, 1), color);
		geometry.MakeQuadInv(0, 1, 2, 3);

		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(x0, y0, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x1, y0, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x1, y1, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x0, y1, box.z.max), float3(0, 0, 1), color);
		geometry.MakeQuad(0, 1, 2, 3);


		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.min, z0), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x0, y0, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x0, y1, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, z0), float3(0, 0, 1), color);
		geometry.MakeQuad(0, 1, 2, 3);


		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.max, box.y.min, z0), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x1, y0, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(x1, y1, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, z0), float3(0, 0, 1), color);
		geometry.MakeQuadInv(0, 1, 2, 3);
	}


	static void MakeCylinder3(Geometry&geometry, const float3&center, float radius, float height, const float4&color = float4(1))
	{
		geometry.SetVertexOffsetToCurrent();
		const count_t Res = 30;
		geometry.MakeVertex(center + float3(0, 0, height), float3(0, 0, 1), color);
		const float h0 = height * 0.9f;
		const float r0 = radius * 0.95f;
		for (index_t i = 0; i < Res; i++)
		{
			index_t j = (i + 1) % Res;
			float a0 = float(i) / Res * 2.f * M_PIF;
			float a1 = float(j) / Res * 2.f * M_PIF;
			float sn = sin(a0);
			float cs = cos(a0);
			geometry.MakeVertex(center + float3(sn*radius, cs*radius, 0), float3(sn, cs, 0), color);
			geometry.MakeVertex(center + float3(sn*radius, cs*radius, h0), float3(sn, cs, 0), color);
			geometry.MakeVertex(center + float3(sn*radius, cs*radius, h0), float3(0, 0, 1), color);
			geometry.MakeVertex(center + float3(sn*r0, cs*r0, height), float3(0, 0, 1), color);

			geometry.MakeQuadInv(1 + i * 4, 1 + i * 4 + 1, 1 + j * 4 + 1, 1 + j * 4);
			geometry.MakeQuadInv(1 + i * 4 + 2, 1 + i * 4 + 1 + 2, 1 + j * 4 + 1 + 2, 1 + j * 4 + 2);
			geometry.MakeTriangle(0, 1 + i * 4 + 3, 1 + j * 4 + 3);
		}
	}


	void MakeCylinder(Geometry&geometry, const float3&center, float radius, float height, const float4&color = float4(1))
	{
		geometry.SetVertexOffsetToCurrent();
		const count_t Res = 30;
		geometry.MakeVertex(center + float3(0, 0, height), float3(0, 0, 1), color);
		for (index_t i = 0; i < Res; i++)
		{
			index_t j = (i + 1) % Res;
			float a0 = float(i) / Res * 2.f * M_PIF;
			float a1 = float(j) / Res * 2.f * M_PIF;
			float sn = sin(a0);
			float cs = cos(a0);
			geometry.MakeVertex(center + float3(sn*radius, cs*radius, 0), float3(sn, cs, 0), color);
			geometry.MakeVertex(center + float3(sn*radius, cs*radius, height), float3(sn, cs, 0), color);
			geometry.MakeVertex(center + float3(sn*radius, cs*radius, height), float3(0, 0, 1), color);

			geometry.MakeQuadInv(1 + i * 3, 1 + i * 3 + 1, 1 + j * 3 + 1, 1 + j * 3);
			geometry.MakeTriangle(0, 1 + i * 3 + 2, 1 + j * 3 + 2);
		}
	}

	void MakeBox(Geometry&geometry, const Box<>&box, const float4&color = float4(1))
	{
		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.min), float3(-1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.min), float3(-1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.max), float3(-1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.max), float3(-1, 0, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);

		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.min), float3(1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.min), float3(1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.max), float3(1, 0, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.max), float3(1, 0, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);


		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.min), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.min), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.max), float3(0, 1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.max), float3(0, 1, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);


		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.min), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.min), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.max), float3(0, -1, 0), color);
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.max), float3(0, -1, 0), color);
		geometry.MakeQuad(0, 1, 2, 3);

		geometry.SetVertexOffsetToCurrent();
		geometry.MakeVertex(float3(box.x.min, box.y.min, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(box.x.max, box.y.min, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(box.x.max, box.y.max, box.z.max), float3(0, 0, 1), color);
		geometry.MakeVertex(float3(box.x.min, box.y.max, box.z.max), float3(0, 0, 1), color);
		geometry.MakeQuad(0, 1, 2, 3);

	}



	void				Geometry::ComputeNormals()
	{
		foreach (this->lods, lod)
		{
			auto&vtx = lod->vertices;
			auto&idx = lod->indices;
			foreach(vtx, v)
				Vec::clear(v->normal);
			for (index_t i = 0; i < idx.Count(); i += 3)
			{
				TVec3<> n;
				Vertex&v0 = vtx[idx[i]],
					&v1 = vtx[idx[i + 1]],
					&v2 = vtx[idx[i + 2]];
				Obj::triangleNormal(v0.vertex, v1.vertex, v2.vertex, n);
				v0.normal += n;
				v1.normal += n;
				v2.normal += n;
			}
			foreach(vtx, v)
				Vec::normalize0(v->normal);
		}

	}







	void Geometry::LoadFromObj(const PathString&filename)
	{
		Clear();
		CGS::Geometry<>	geo;
		PathString baseName = FileSystem::ExtractFileDirName(filename);
		PathString cgsName = baseName + ".cgs";
		if (!FileSystem::IsFile(cgsName) || FileSystem::GetModificationTime(cgsName) < FileSystem::GetModificationTime(filename) || !geo.LoadFromFile(cgsName))
		{
			LogEvent(EventSignificance::Uncommon, "Reconverting " + String(filename)+"...");
			Converter::obj_converter.positionObjects = false;
			ASSERT1__(Converter::obj_converter.LoadFromFile(geo, filename),filename);
			geo.SaveToFile(cgsName);
		}
		bool generateNormals = false;
		foreach(geo.material_field, material)
		{
			const float4 color = material->info.diffuse;
				//0.9f * (1.f - (1.f - material->info.diffuse) * 0.75f) + 0.1f;
			foreach(material->data.object_field, obj)
			{
				this->SetLOD(obj->detail);
				//obj->detail
				this->SetVertexOffsetToCurrent();

				const count_t floatsPerVertex = obj->vpool.vsize();
				const bool hasNormal = (obj->vpool.vflags & CGS::HasNormalFlag) != 0;
				const bool hasColor = (obj->vpool.vflags & CGS::HasColorFlag) != 0;
				ASSERT__(!hasColor);	//obj doesn't support color

				const float*source = obj->vpool.vdata.pointer();
				const count_t vertexCount = obj->vpool.vcnt;
				const float*const sourceEnd = source + floatsPerVertex * vertexCount;

				if (!hasNormal)
					generateNormals = true;

				for (; source != sourceEnd; source += floatsPerVertex)
				{
					//if (hasNormal)
					//{
					//	//if (hasColor)
					//	//	geometry.MakeVertex(Vec::ref3(source), Vec::ref3(source + 3), Vec::ref4(source + 6));
					//	//else
					//	geometry.MakeVertex(Vec::ref3(source), Vec::ref3(source + 3), material->info.diffuse);
					//}
					//else
					{
						this->MakeVertex(Vec::ref3(source), float3(), color);
					}
				}


				this->AppendTriangleIndices(obj->ipool.idata.pointer(), obj->ipool.triangles * 3);
				this->AppendQuadIndices(obj->ipool.idata.pointer() + obj->ipool.triangles * 3, obj->ipool.quads*4);
			}
		}

		this->GenerateNormals();
		UpdateBoundingBox();
	}


	const Box<>&			Geometry::GetBoundingBox()	const
	{
		return boundingBox;

	}

	void			Geometry::UpdateBoundingBox()
	{
		boundingBox = Box<>::Invalid();

		if (!CountLODs())
			return;

		const auto&lod = GetLOD(CountLODs() - 1);

		foreach (lod.vertices,vtx)
			boundingBox.Include(vtx->vertex);

	}


	bool			Geometry::TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance)	const
	{
		if (!CountLODs())
			return false;

		float d2 =distance;
		if (!_oDetectOpticalBoxIntersection(boundingBox,b,d,d2))
			return false;
		//Mat::transform(system,b,test);
		bool rs = false;
		const auto&lod = GetLOD(CountLODs() - 1);
		const Vertex*vtx = lod.vertices.pointer();
		//TVec3<> _p0,_p1,_p2;


		for (index_t i = 0; i < lod.indices.Count(); i += 3)
		{
			if (Obj::detectOpticalIntersection(
				vtx[lod.indices[i]].vertex,
				vtx[lod.indices[i + 1]].vertex,
				vtx[lod.indices[i + 2]].vertex, b, d, distance))
			{
				rs = true;
			}
		}

		return rs;
	}


}

