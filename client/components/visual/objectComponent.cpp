#include <global_root.h>
#include "objectComponent.h"
#include "objectRendering.h"
#include <objects/structure/vehicleStore.h>
#include <objects/structure/vehiclePlant.h>
#include <objects/structure/mine.h>
#include <objects/structure/laboratory.h>
#include "display.h"

#include <objects/prototypes.h>

namespace Objects
{
	DX11::IBO32					ParaboloidBuffer::attackIBO;
	DX11::BaseInstructionBlock	ParaboloidBuffer::paraboloidShader;

	/*static*/ void		ParaboloidBuffer::StaticDestroy()
	{
		attackIBO.Clear();
		paraboloidShader.Clear();
	}


	/*static*/ void ParaboloidBuffer::StaticCreate()
	{
		DX11::Shader::Binding bindings2[] =
		{
			DX11::Shader::Binding(overallTint, "Tint"),
			//DX11::Shader::Binding(hexGrid, "Texture"),
			//DX11::Shader::SamplerBinding(false,Engine::VisualEnum::TextureFilter::Trilinear,4,"Sampler"),
		};

		paraboloidShader.binding = stdVehicleShader.binding;
		paraboloidShader.shader.LoadFromFile("shaders/attackParaboloid.hlsl", "vsMain", "psMain", bindings2, ARRAYSIZE(bindings2));
		paraboloidShader.UpdateLayout();
		paraboloidShader.SetTriangles();
	}


	/*static*/ void	ParaboloidBuffer::MakeHull(Engine::DX11::InstructionBlock32&target, const TMatrix4<>&matrix, float flatRange, float explosionRadius, const TVec3<>&center)
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
	void TowerAttackVisualization::Recompile(const Tower & t, const Grid::LCellIdI & attackTarget)
	{
		static Buffer0<TVec4<> > trajectory;
		trajectory.Clear();
		TVec3<> impactAt;
		Grid::HCellIdI dummy1;
		float angle;
		//if (Turret::TraceTrajectory(Turret::MaxRangeToVelocity(GetFlatAttackRange()),World::actual, location,foundationHeight,attackTarget,trajectory,impactAt,dummy1))
		if (t.TraceTrajectory(TrajectoryEndPoint( attackTarget,0),trajectory,impactAt,dummy1,angle) != TurretTrajectory::InvalidTrajectory)
		{

			static Geometry geo;
			geo.Clear();

			Component::Turret::CompileTrajectory(trajectory,Component::Turret::Schematic,geo,0.25f);

			if (trajectoryVisualization.shader.IsEmpty())
				(DX11::BaseInstructionBlock&)trajectoryVisualization = pathShader;
			trajectoryVisualization.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
			trajectoryVisualization.ibo.Create(geo.GetLOD(0).indices);


			((DX11::BaseInstructionBlock&) blastVisualization) = jobShader;
			blastVisualization.vbo = sphereVBO[0];
			blastVisualization.ibo = sphereIBO[0];
			//shape.depthTest = true;

			t.turret->BuildBlastSystem(trajectory.last().xyz * trajectory.last().w,t.turret->GetExplosionRadius(t.owner),blastSystem);
		}
			
	}


	/*static*/ void				StructureHull::LoadAll()
	{
		LoadFor<Objects::Mine>();
		LoadFor<Objects::MineralContainer>();
		LoadFor<Objects::FuelContainer>();
		LoadFor<Objects::UraniumStorage>();
		LoadFor<Objects::MunitionStorage>();
		LoadFor<Objects::MunitionFactory>();
		LoadFor<Objects::VehiclePlant>();
		LoadFor<Objects::VehicleStore>();
		LoadFor<Objects::Laboratory>();
		LoadFor<Objects::Connector>();
		LoadFor("tower",Objects::Tower::geometry,Objects::Tower::staticShape);	//special case, since there is one variant per applicable turret, but all have the same base

		float h = Objects::Tower::geometry.GetBoundingBox().z.max;
		ASSERT2__(similar(h,Objects::Tower::foundationHeight),h,Objects::Tower::foundationHeight);
		Objects::Tower::foundationHeight = h;


		Objects::Generator::reactorConnector.LoadFromObj("objects/reactorCoreConnector.obj");
		Objects::VehicleStore::interConnector.LoadFromObj("objects/vehicleStoreConnector.obj");

	}


	/*static*/ void				StructureHull::LoadFor(const Objects::Structure&prototype, Objects::Geometry&staticGeometry, PClientAttachment&staticShape)
	{
		LoadFor(prototype.GetName(),staticGeometry,staticShape);
	}

	/*static*/ void				StructureHull::LoadFor(const String&geometryName, Objects::Geometry&staticGeometry, PClientAttachment&staticShape)
	{
		std::shared_ptr<StructureHull> shape(new StructureHull());
		staticShape = shape;
		String name = geometryName;
		name.set(0, Template::tolower(name.firstChar()));
		name.replace(' ',"");
		staticGeometry.Clear();
		staticGeometry.LoadFromObj(PathString("objects/"+name+".obj"));

		if (shape->constShape.shader.IsEmpty())
			(DX11::BaseInstructionBlock&)shape->constShape = standardLocalizableShader;
		shape->constShape.vbo.CreateFromStructs(staticGeometry.GetLOD(0).vertices);
		shape->constShape.ibo.Create(staticGeometry.GetLOD(0).indices);
	}

	void				ConnectorHull::RecompileConnectors(Objects::Structure&structure)
	{
		structure.changed = false;

		LogEvent(EventClass::Preset::common, &structure, "Recompiling geometry");
		localMesh.ClearCompletely();

		{
			for (index_t i = 0; i < structure.connectedTo.Count(); i++)
			{
				PStructure c = structure.connectedTo[i].lock();
				if (!c)
				{
					LogEvent(EventClass::Preset::error,&structure,"Removing lost neighbor connection #"+String(i));
					structure.connectedTo.Erase(i);
					i--;
					continue;
				}
					
				if (c->location.stackID > structure.location.stackID)
					continue;	//only one direction

				const Geometry*source = structure.GetSourceForConnection(c);
				if (!source)
					continue;

				for (index_t lod = 0; lod < source->CountLODs(); lod++)
				{
					const Geometry::LOD&data = source->GetLOD(lod);
					const count_t numVertices = data.vertices.Count();
					//const Vertex*vertexData = data.vertices.pointer();
					localMesh.SetLOD(lod);

					localMesh.SetVertexOffsetToCurrent();
					//const float3&b0 = Grid::low.cells[this->location.stackID].cornerCenter;
					//const float3&b1 = Grid::low.cells[(*c)->location.stackID].cornerCenter;

					const TVec3<>&center0 = structure.GetWorldPosition(),
								&center1 = c->GetWorldPosition();

					const float height0 = Vec::length(center0),
								height1 = Vec::length(center1),
								heightDelta = height1 - height0;
//						const float3 d = b1 - b0;
					const float3 centerDelta = center1 - center0;
					//const float scale = d.Length();
					const float distance = centerDelta.Length();
					//const float3 d1 = d / scale;
					const float3 normalizedDelta = centerDelta / distance;
					//const float r0 = Grid::ToRadius(location.height) + 0.1f;
					//const float r1 = Grid::ToRadius(location.height + 1) + 0.1f;
					//const float rd = r1 - r0;
					//const float r1y = Grid::ToRadius((*c)->location.height)+0.1f;
					//const float rdy = r1y - r0;
					const float l0 = GetLowLighting(structure.location + structure.GetHeight()-1);
					const float l1 = GetLowLighting(c->location + structure.GetHeight()-1);
					const float ld = l1 - l0;
					//const float3 x = -(b0 | d1).Normalized();// * (r1 - r0);
					const float3 x = -(float3::Reinterpret(center0) | centerDelta).Normalized();// * (r1 - r0);
					const float horzScaleBase = Vec::length(structure.system.x.xyz);
					const float horzScaleDelta = Vec::length(c->system.x.xyz) - horzScaleBase;
					for (index_t i = 0; i < numVertices; i++)
					{
						//position.y [0,1] is progress from this structure towards neighbor c
						//position.x [-1,1] is horizontal placement of vertical along orthogonal axis
						//position.z [0,1] is vertical placement along radial axis
						const Vertex&vtx = data.vertices[i];
						const TVec3<>&position = vtx.vertex;
						//float r = r0 + rdy * position.y + (rd) * position.z;
						//TVec3<> np = b0 + d * position.y;
						TVec3<> np = center0 + centerDelta * position.y;
						Vec::normalize(np);
						np *= height0 + heightDelta * position.y + Grid::SingleLayerHeight * position.z;
						np += x * position.x * (horzScaleBase + horzScaleDelta * position.y);
						float light = l0 + (ld)* position.y;
						localMesh.MakeVertex(np, float3(), float4(vtx.color.rgb, light));
					}
					localMesh.AppendTriangleIndices(data.indices);
				}
			}
			localMesh.ComputeNormals();
		}
		if (localMesh.CountLODs())
		{

			//if (compiledShape.shader.IsEmpty())
				(DX11::BaseInstructionBlock&)compiledShape = Objects::standardCompiledShader;
			compiledShape.vbo.CreateFromStructs(localMesh.GetLOD(0).vertices);
			compiledShape.ibo.Create(localMesh.GetLOD(0).indices);
		}
		else
		{
			compiledShape.vbo.Clear();
			compiledShape.ibo.Clear();
		}
	}


	void			VisualComponent::FinishCreation(Objects::Component::Base&base, bool scaleToVehicleSize/*=true*/)
	{
		partInstruction.SetSize(base.parts.Count());
		for (index_t i = 0; i < base.parts.Count(); i++)
		{
			auto&part = base.parts[i];
			auto&inst = partInstruction[i];
			if (scaleToVehicleSize)
				part.Scale(vehicleScale);
			(DX11::BaseInstructionBlock&)inst = stdVehicleShader;
			inst.vbo.CreateFromStructs(part.GetLOD(0).vertices);
			inst.ibo.Create(part.GetLOD(0).indices);
		}
	}

	/*static*/ void		VisualComponent::FinalizeAll()
	{
		foreach (Objects::Component::allTurretComponents,t)
			Allocate(**t)->FinishCreation(**t,true);
		foreach (Objects::Component::baseComponents,b)
			Allocate(**b)->FinishCreation(**b,true);
	}
	


}
