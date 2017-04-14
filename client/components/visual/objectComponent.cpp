#include <global_root.h>
#include "visualObjectComponent.h"

namespace Objects
{




	#ifdef HAS_WINDOW





		/*static*/ void	ParaboloidBuffer::MakeHull(DX11::InstructionBlock32&target, const TMatrix4<>&matrix, float flatRange, float explosionRadius, const TVec3<>&center)
		{
			float effectiveRange = (flatRange);

			float height = Vec::length(center);

			//factor = 0;
			static Geometry geo;
			geo.Clear();


			static const int RadialResolution = 16;
			static const int AngularResolution = 32;
			float3 normal = float3(0,0,1);
			float3 vtx = float3(0,0,0.5f * effectiveRange + height + explosionRadius);
			//Mat::rotate(matrix,vtx);
			//Mat::rotate(matrix,normal);
			geo.MakeVertex(vtx,normal,float3(0));
			for (int ir = 1; ir < RadialResolution; ir++)
			{
				float fr = float(ir)/float(RadialResolution-1);
				float fr0 = fr * 2.f * flatRange;


				float2 p (fr0,height);
				Vec::setLen(p,height);
				float fr1 = Vec::distance(p,float2(0,height));
				//float error = p.Length() - height;
					

				float z = Component::Turret::MaxHeightAtHDistance(effectiveRange,fr1);




				float slope = Component::Turret::MaxHeightSlopeAtHDistance(effectiveRange,fr1);
				float r = sqrt(sqr(slope) + 1.f);
				slope /= r;
				float rest = sqrt(1.f - sqr(slope));

				for (int ia = 0; ia < AngularResolution; ia++)
				{
					float fa = float(ia) / float(AngularResolution);
					float a =  fa * 2.f * M_PIF;
					float	cs = cos(a),
							sn = sin(a);
					float3 normal = float3(cs*-slope,sn*-slope,rest);
					float3 vtx = float3(cs*fr0,sn*fr0,height);
					Vec::setLen(vtx,z+height);
					vtx += normal * explosionRadius;
					//Mat::rotate(scaled,vtx);
					//Mat::rotate(scaled,normal);
					geo.MakeVertex(vtx /*+ normal * factor*/,normal,float3(fr));
					if (attackIBO.IsEmpty())
					{
						if (ir > 1)
							geo.MakeQuad(ia,(ia+1)%AngularResolution,(ia+1)%AngularResolution-AngularResolution,ia-AngularResolution);
						else
							geo.MakeTriangle(ia+1,(ia+1)%AngularResolution+1,0);
					}
				}
				geo.SetVertexOffsetToCurrent();
			}

			if (attackIBO.IsEmpty())
			{
				target.ibo.Create(geo.GetLOD(0).indices);
				attackIBO = target.ibo;
			}
			else
				target.ibo = attackIBO;
			target.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
		}

		void ParaboloidBuffer::Render(const Grid::LCellIdI&cell, float zOffset, float flatRange, float explosionRadius, const float4&color, bool extended)
		{
			const TVec3<>&normalizedUp = Grid::low.cells[cell.stackID].centerVertex.xyz;


			const TVec3<> center = normalizedUp * (zOffset + Grid::ToRadius(cell.height));

			//float d = Vec::distance(scene.GetAbsoluteLocation(),center);
			//int instructionIndex = DetermineLODInstruction(radius / d);

//			float factor = explosionRadius / flatRange;

			if (attackInstruction.shader.IsEmpty())
			{
				attackInstruction.SetBase(paraboloidShader);
				extendedInstruction.SetBase(paraboloidShader);
			}

			if (!Vec::equal(lastCenter ,center) || flatRange != lastRange || lastExp != explosionRadius)
			{
				lastCenter = center;
				lastRange = flatRange;
				lastExp = explosionRadius;
				//lastFactor = factor;

				Mat::makeAxisSystem(float3(), normalizedUp,2,matrix);
				MakeHull(attackInstruction, matrix, flatRange,explosionRadius,center);
				MakeHull(extendedInstruction, matrix, flatRange,explosionRadius + Renderer::RangeShadowExtension,center);
			}


			//scaled.x *= flatRange;
			//scaled.y *= flatRange;
			//scaled.z *= flatRange;

			overallTint.Update(color);
				
			display.SetObjectMatrix(matrix);
			display.Render(extended ? extendedInstruction : attackInstruction,true);
		}

	#endif
}
