#include <global_root.h>
#include "turretBase.h"
#include "rendering.h"

#include "gridAndChoices.h"


#ifdef HAS_WINDOW
	#include <engine/display.h>
	extern Engine::Display<Engine::DirectX11>	display;
#endif

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


	


	/*static*/ bool				TurretBase::CheckArea(const OwnerParameter&myFaction, const Grid::LCellIdI&origin, float explosionRadius)
	{
		AreaHelper	helper;

		
		{
			const auto&n = Grid::low.grid.GetCell(origin.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				helper << Grid::LCellIdI(n.neighborID[i],origin.height);
			if (origin.height+1 < Grid::CellStack::HalfRange)
				helper << origin + 1;
			if (origin.height-1 >= -Grid::CellStack::HalfRange)
				helper << origin - 1;
		}
		Grid::LCellIdI current;
		const TVec3<> source3 = Grid::ToWorld(origin);
		float range2 = sqr(explosionRadius);

		while (helper >> current)
		{

			TVec3<> world = Grid::ToWorld(current);
			float fromSource2 = Vec::quadraticDistance(source3,world);
			if (fromSource2 > range2)
				continue;


			PWorldEntity e = GetEntity(current);
			if (e && e->owner == myFaction && !e->IsIndestructible())
				return false;


			{
				const auto&n = Grid::low.grid.GetCell(current.stackID).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					helper << Grid::LCellIdI(n.neighborID[i],current.height);
				if (current.height+1 < Grid::CellStack::HalfRange)
					helper << current + 1;
				if (current.height-1 >= -Grid::CellStack::HalfRange)
					helper << current - 1;
			}

		}
		return true;
	
	}



	bool					TurretBase::ExtCanHit(const OwnerParameter&myFaction, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, Grid::LCellIdI&destOut)	const
	{
		//const float velocity = Turret::MaxRangeToVelocity(GetFlatAttackRange());
		const float range = GetFlatAttackRange();
		//const float extRange = range + (turret->explosionRadius);
		//const float range2 = sqr(range);
		//const float extRange2 = sqr(extRange);
		//const TVec3<> source3 = Grid::ToWorld(source);
		//TVec3<> dest3 = Grid::ToWorld(destIn);
		//float distance2 = Vec::quadraticDistance(source3,dest3);
		//if (Turret::GetAngleForDistance(velocity,source,destIn))
		//if (distance2 > extRange2)
		//	return false;
		const float explosionRadius = turret->explosionRadius;
		const float explosionRadius2 = sqr(explosionRadius);
		if (Component::Turret::CanHit(turret->trajectoryType, range,source,destIn)
			&& CheckArea(myFaction,destIn.cellLocation,explosionRadius))
		{
			destOut = destIn.cellLocation;
			return true;
		}

		if (!Component::Turret::CanHit(turret->trajectoryType, range+explosionRadius*2,source,destIn))
			return false;

		AreaHelper	helper;
		//queue << destIn;
		{
			const auto&n = Grid::low.grid.GetCell(destIn.cellLocation.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				helper << Grid::LCellIdI(n.neighborID[i],destIn.cellLocation.height);
			helper << destIn.cellLocation + 1;
			helper << destIn.cellLocation - 1;
		}
		Grid::LCellIdI current;
		while (helper >> current)
		{
			TVec3<> world = Grid::ToWorld(current);
			float fromDest2 = Vec::quadraticDistance(destIn.worldLocation,world);
			if (fromDest2 > sqr(turret->explosionRadius))
				continue;
			if (!Grid::actualWorld.LowCellIsClear(current) || 
				!Component::Turret::CanHit(turret->trajectoryType, range,source,TrajectoryEndPoint( current,0)))
				continue;
			//float fromSource2 = Vec::quadraticDistance(source3,world);
			//if (fromSource2 > extRange2)
				//continue;



			if (CheckArea(myFaction,current,explosionRadius))
			{
				destOut = current;
				return true;
			}
			{
				const auto&n = Grid::low.grid.GetCell(current.stackID).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					helper << Grid::LCellIdI(n.neighborID[i],current.height);
				helper << current + 1;
				helper << current - 1;
			}
		}
		return false;
	}



}
