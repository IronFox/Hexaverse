#include <global_root.h>
#include "objectComponent.h"
#include "objectRendering.h"
#include <objects/structure/vehicleStore.h>
#include <objects/structure/vehiclePlant.h>
#include <objects/structure/planeFactory.h>
#include <objects/structure/mine.h>
#include <objects/structure/laboratory.h>
#include <objects/subOrbital.h>
#include <objects/systems.h>
#include "../display.h"

#include <objects/prototypes.h>

namespace Objects
{
	DX11::IBO32					ParaboloidBuffer::attackIBO;
	DX11::BaseInstructionBlock	ParaboloidBuffer::paraboloidShader;



	template <typename T>
		static T&StandardOf(const PClientAttachment&attach)
		{
			DBG_ASSERT__(To<T>::DynamicCast( attach ));	//have to remove this later on
			return *(T*)attach.get();
		}
	template <typename T>
		static T&StandardAllocate(PClientAttachment&attach)
		{
			if (!attach)
				attach.reset(new T());
			else
			{
				DBG_ASSERT__(To<T>::DynamicCast( attach ));	//have to remove this later on
			}
			return *(T*)attach.get();
		}

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


	/*static*/ void	ParaboloidBuffer::MakeHull(Engine::DX11::InstructionBlock32&target, const M::TMatrix4<>&matrix, float flatRange, float explosionRadius, const M::TVec3<>&center)
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
			//float error = p.GetLength() - height;
					

			float z = Component::Turret::MaxHeightAtHDistance(effectiveRange,fr1);




			float slope = Component::Turret::MaxHeightSlopeAtHDistance(effectiveRange,fr1);
			float r = sqrt(M::Sqr(slope) + 1.f);
			slope /= r;
			float rest = sqrt(1.f - M::Sqr(slope));

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
		const M::TVec3<>&normalizedUp = Grid::low.cells[cell.stackID].centerVertex.xyz;


		const M::TVec3<> center = normalizedUp * (zOffset + Grid::ToRadius(cell.height));

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
		static Buffer0<M::TVec4<> > trajectory;
		trajectory.Clear();
		//if (Turret::TraceTrajectory(Turret::MaxRangeToVelocity(GetFlatAttackRange()),World::actual, location,foundationHeight,attackTarget,trajectory,impactAt,dummy1))
		if (t.TraceTrajectory(TrajectoryEndPoint( attackTarget,0),true,trajectory) != TraceResult::InvalidTrajectory)
		{

			static Geometry geo;
			geo.Clear();

			Component::Turret::CompileTrajectory(trajectory,Component::Turret::Schematic,geo,0.25f);

			if (trajectoryVisualization.shader.IsEmpty())
				(DX11::BaseInstructionBlock&)trajectoryVisualization = pathShader;
			trajectoryVisualization.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
			trajectoryVisualization.ibo.Create(geo.GetLOD(0).indices);


			((DX11::BaseInstructionBlock&) blastVisualization) = jobShader;
			blastVisualization.vbo = Renderer::sphere.vbo[0];
			blastVisualization.ibo = Renderer::sphere.ibo[0];
			//shape.depthTest = true;

			t.turret->BuildBlastSystem(trajectory.Last().xyz * trajectory.Last().w,t.turret->GetExplosionRadius(t.owner),blastSystem);
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
		LoadFor<Objects::PlaneFactory>();
		LoadFor<Objects::VehicleStore>();
		LoadFor<Objects::Laboratory>();
		LoadFor<Objects::Connector>();
		LoadFor<Objects::Generator>();
		LoadFor<Objects::ReactorCore>();
		LoadFor<Objects::NexusPort>();
		LoadFor("tower",Objects::Tower::geometry,Objects::Tower::staticShape);	//special case, since there is one variant per applicable turret, but all have the same base
		foreach (Objects::Tower::globalPrototypes,p)
			(*p)->instance->shape = Objects::Tower::staticShape;

		LoadFor<Objects::SubOrbital::Raptor>();
		LoadFor<Objects::SubOrbital::CargoPlane>();
		LoadFor<Objects::SubOrbital::SensorDrone>();

		float h = Objects::Tower::geometry.GetBoundingBox().z.max;
		ASSERT2__(M::Similar(h,Objects::Tower::foundationHeight),h,Objects::Tower::foundationHeight);
		Objects::Tower::foundationHeight = h;


		Objects::Generator::reactorConnector.LoadFromObj("objects/reactorCoreConnector.obj");
		Objects::VehicleStore::interConnector.LoadFromObj("objects/vehicleStoreConnector.obj");

	}


	/*static*/ void				StructureHull::LoadFor(Objects::SubOrbital::Plane&prototype, Objects::Geometry&staticGeometry, PClientAttachment&staticShape)
	{
		LoadFor(prototype.GetTypeName(), staticGeometry, staticShape);
		prototype.shape = staticShape;
	}

	/*static*/ void				StructureHull::LoadFor(Objects::Structure&prototype, Objects::Geometry&staticGeometry, PClientAttachment&staticShape)
	{
		LoadFor(prototype.GetTypeName(),staticGeometry,staticShape);
		prototype.shape = staticShape;
	}

	/*static*/ void				StructureHull::LoadFor(const String&geometryName, Objects::Geometry&staticGeometry, PClientAttachment&staticShape)
	{
		std::shared_ptr<StructureHull> shape(new StructureHull());
		staticShape = shape;
		String name = geometryName;
		name.Set(0, CharFunctions::tolower(name.FirstChar()));
		name.FindAndReplace(' ',"");
		staticGeometry.Clear();
		staticGeometry.LoadFromObj(PathString("objects/"+name+".obj"));

		if (shape->constShape.shader.IsEmpty())
			shape->constShape = standardLocalizableShader;
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

					const M::TVec3<>&center0 = structure.GetWorldPosition(),
								&center1 = c->GetWorldPosition();

					const float height0 = Vec::length(center0),
								height1 = Vec::length(center1),
								heightDelta = height1 - height0;
//						const float3 d = b1 - b0;
					const float3 centerDelta = center1 - center0;
					//const float scale = d.GetLength();
					const float distance = centerDelta.GetLength();
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
						const M::TVec3<>&position = vtx.vertex;
						//float r = r0 + rdy * position.y + (rd) * position.z;
						//M::TVec3<> np = b0 + d * position.y;
						M::TVec3<> np = center0 + centerDelta * position.y;
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
			compiledShape = Objects::standardCompiledShader;
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
			inst = stdVehicleShader;
			inst.vbo.CreateFromStructs(part.GetLOD(0).vertices);
			inst.ibo.Create(part.GetLOD(0).indices);
		}
	}

	/*static*/ void		VisualComponent::FinalizeAll()
	{
		foreach (Objects::Component::allTurretComponents,t)
			Allocate(**t).FinishCreation(**t,true);
		foreach (Objects::Component::baseComponents,b)
			Allocate(**b).FinishCreation(**b,true);
	}
	

	void					TaskAttachment::RecompilePath(const MobileEntity::Task&t, int stepsPerRound, float structureSize/*=vehicleScale*/)
	{
		Geometry geo;

		struct TNode
		{
			float3	dir,
					normal;
			bool	stopHere;
		};

		Array<TNode>		nodes(t.path.GetLength());
		
		int remaining = stepsPerRound;

		for (index_t i = 0; i+1 < t.path.Count(); i++)
		{
			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = t.path[i].cell,
								&b = t.path[i+1].cell;
			if (remaining >= t.path[i+1].movementCostsFromPrevNode)
			{
				remaining -= t.path[i+1].movementCostsFromPrevNode;
				na.stopHere = false;
			}
			else
			{
				remaining = stepsPerRound;
				remaining -= t.path[i+1].movementCostsFromPrevNode;
				na.stopHere = true;
			}
			float3 p0 = Grid::low.cells[a.stackID].centerVertex.xyz * Grid::ToRadius(a.height);
			float3 p1 = Grid::low.cells[b.stackID].centerVertex.xyz * Grid::ToRadius(b.height);
			M::TVec3<> n,d = p1 - p0;
			Vec::cross(p0,d,n);
			na.normal += n;
			nb.normal += n*0.9f;
			na.dir += d;
			nb.dir += d*0.9f;
		}
		foreach (nodes, n)
		{
			Vec::setLen0(n->dir,structureSize);
			Vec::setLen0(n->normal,structureSize);
		}



		for (index_t i = 0; i+1 < t.path.Count(); i++)
		{
			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = t.path[i].cell,
								&b = t.path[i+1].cell;
			float3 d0 = Grid::low.cells[a.stackID].centerVertex.xyz;
			float3 p0 = d0  * Grid::ToRadiusf(float(a.height)+0.5f);
			float3 d1 = Grid::low.cells[b.stackID].centerVertex.xyz;
			float3 p1 = d1 * Grid::ToRadiusf(float(b.height)+0.5f);

			float3 c0 = p0 + na.dir;
			float3 c1 = p1 - nb.dir;


			bool edge = i+2 < nodes.Count() && nodes[i+1].stopHere ;

			//!((i+1)% stepsPerRound);

			//M::TVec3<> d = p1 - p0;
			//if (a.height == b.height)

			static const int Res = 8;
			for (int j = (i!=0); j <= Res; j++)
			{
				float fj = float(j) / float(Res);
				M::TVec3<> p,n,d;

				float op = i > 0 ? 1.f : fj;
				
				Vec::resolveBezierCurvePoint(p0,c0,c1,p1,fj,p);
				//Vec::resolveBezierCurveAxis(p0,c0,c1,p1,fj,d);
				//Vec::cross(p,d,n);
				Vec::interpolate(na.normal,nb.normal,fj,n);
				Vec::setLen(n,structureSize);
				if (j > 0)
				{
					geo.MakeQuad(0,1,4,3);
					geo.MakeQuad(1,2,5,4);

				}
				//if (!i)
				//{
				//	geo.MakeVertex(p0 + direction[i],d0,float3(0));
				//	geo.MakeVertex(p0 - direction[i],d0,float3(1));
				//}

				geo.SetVertexOffsetToCurrent();

				{
					geo.MakeVertex(p + n,d1,float3(op,1,1));
					if (j < Res || !edge)
						geo.MakeVertex(p,d1,float3(0));
					else
						geo.MakeVertex(p,d1,float3(op,1,1)*0.7f);
					geo.MakeVertex(p - n,d1,float3(op,1,1));
				}
				if (i+2 == t.path.Count() && fj > 0.5f)
				{
					//geo.SetVertexOffsetToCurrent();
					//geo.MakeVertex(p + n*2.f,d1,float3(0));
					//geo.MakeVertex(p - n*2.f,d1,float3(1));
					break;
				}
			}
		}
		if (t.path.Count() > 1)
		{
			const Grid::LCellIdI&b = t.path.Last().cell;

			float3 d1 = Grid::low.cells[b.stackID].centerVertex.xyz;
			float3 p1 = d1 * Grid::ToRadiusf(float(b.height)+0.5f);

			//float s = std::max(0.f,2.f - (fj-0.5f)*4.f);
			geo.MakeVertex(p1,d1,float3(1));
			geo.MakeTriangle(0,1,3);
			geo.MakeTriangle(1,2,3);
		}

		//geo.Scale(vehicleScale);
		if (instruction.shader.IsEmpty())
			(DX11::BaseInstructionBlock&)instruction = pathShader;
		instruction.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
		instruction.ibo.Create(geo.GetLOD(0).indices);
	}


	
	void						TaskAttachment::SetLocalizableShape(EntityType shape, const Grid::LCellIdI&location, const OwnerParameter&owner)
	{
		localizableShapes.Clear();
		if (shape == ConstName::DetonationSize_Regular)
		{
			Shape&shape = localizableShapes.Append();
			((DX11::BaseInstructionBlock&) shape.instruction) = jobShader;
			shape.instruction.vbo = Renderer::sphere.vbo[1];
			shape.instruction.ibo = Renderer::sphere.ibo[1];
			shape.depthTest = true;
			shape.flareDisplay = false;
			shape.preRenderDepth = false;
			shape.selectedOnly = true;

			float r = Component::Bomb::GetRadius(false,owner);
			BuildSystem(Grid::ToWorld(location),r, r,shape.system);
			return;
		}

		if (shape == ConstName::DetonationSize_Doomsday)
		{
			Shape&shape = localizableShapes.Append();
			((DX11::BaseInstructionBlock&) shape.instruction) = jobShader;
			shape.instruction.vbo = Renderer::sphere.vbo[2];
			shape.instruction.ibo = Renderer::sphere.ibo[2];
			shape.depthTest = true;
			shape.flareDisplay = false;
			shape.preRenderDepth = false;
			shape.selectedOnly = true;

			float r = Component::Bomb::GetRadius(true,owner);
			BuildSystem(Grid::ToWorld(location),r, r,shape.system);
			return;
		}

		PStructure p = 	Prototypes::structures.Find(shape);
		if (p)
		{
			Shape&shape = localizableShapes.Append();
			((DX11::BaseInstructionBlock&) shape.instruction) = jobShader;

			const auto&strucShape = StructureHull::Of(*p).constShape;
			shape.instruction.vbo = strucShape.vbo;
			shape.instruction.ibo = strucShape.ibo;
			shape.depthTest = true;
			shape.flareDisplay = wasJob;

			BuildStructureSystem(location,shape.system);
			return;
		}
	}

	
	void TaskAttachment::Recompile(const MobileEntity&e, MobileEntity::Task&t,const Grid::LCellIdI&pointOfOrigin)
	{
		auto&attach = Allocate(t);
		attach.wasJob = false;
		switch (t.type)
		{
			case MobileEntity::Task::Job:
			{
				attach.wasJob = true;
				if (!t.job || !t.job->movesToFocusWhenDone || t.foci.IsEmpty())
					break;
				std::shared_ptr<VehicleJob::PathJob> pjob = To<VehicleJob::PathJob>::DynamicCast(t.job);
				//if (foci.IsNotEmpty() || type == MoveTo)

				if (pjob)
				{
					if (t.foci.IsNotEmpty() && pjob->UpdatePath(e, pointOfOrigin, t.foci.Peek().target))
					{
						attach.RecompilePath(t,1);
					}
					break;
				}
			}
			case MobileEntity::Task::MoveTo:
				attach.RecompilePath(t, e.GetMovementStepsPerRound(),e.GetPathSize());
			break;
			case MobileEntity::Task::Mode:
				switch (t.mode)
				{
					case CursorMode::AttackMode:
					{
						static Buffer0<M::TVec4<> > trajectory;
						Geometry geo;
						attach.localizableShapes.Clear();
						foreach (t.foci,f)
						{
							trajectory.Clear();
							if (e.TraceTrajectoryFrom(pointOfOrigin,TrajectoryEndPoint( f->target,0), true,trajectory) != TraceResult::InvalidTrajectory)
							{
								Component::Turret::CompileTrajectory(trajectory,Component::Turret::Schematic,geo,0.25f);

								Shape&shape = attach.localizableShapes.Append();
								((DX11::BaseInstructionBlock&) shape.instruction) = jobShader;
								shape.instruction.vbo = Renderer::sphere.vbo[2];
								shape.instruction.ibo = Renderer::sphere.ibo[2];
								shape.depthTest = true;
								shape.flareDisplay = true;

								BuildSystem(trajectory.Last().xyz * trajectory.Last().w,e.GetAttackExplosionRadius(),  e.GetAttackExplosionRadius(),shape.system);
							}
						}
						if (attach.instruction.shader.IsEmpty())
							(DX11::BaseInstructionBlock&)attach.instruction = pathShader;
						attach.instruction.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
						attach.instruction.ibo.Create(geo.GetLOD(0).indices);
					}
					break;
				}
			break;
		}
	}

	/*static*/ TaskAttachment&	TaskAttachment::Of(const Objects::MobileEntity::Task&t)
	{
		return StandardOf<TaskAttachment>(t.visualization);
	}

	/*static*/ TaskAttachment*	TaskAttachment::FetchOptional(const Objects::MobileEntity::Task&t)
	{
		return (TaskAttachment*)t.visualization.get();
	}

	/*static*/ TaskAttachment&	TaskAttachment::Allocate(Objects::MobileEntity::Task&t)
	{
		return StandardAllocate<TaskAttachment>(t.visualization);
	}

	/*static*/ ConnectorHull&		ConnectorHull::Of(const Objects::Structure&s)
	{
		return StandardOf<ConnectorHull>(s.connectorShape);
	}

	/*static*/ ConnectorHull&		ConnectorHull::Allocate(Objects::Structure&s)
	{
		return StandardAllocate<ConnectorHull>(s.connectorShape);
	}

	/*static*/ VisualComponent&		VisualComponent::Of(const Objects::Component::Base&b)
	{
		return StandardOf<VisualComponent>(b.attachment);
	}
	/*static*/ VisualComponent&		VisualComponent::Allocate(Objects::Component::Base&b)
	{
		return StandardAllocate<VisualComponent>(b.attachment);
	}

	/*static*/ ParaboloidBuffer&	ParaboloidBuffer::Of(const Objects::TurretBase&b)
	{
		return StandardOf<ParaboloidBuffer>(b.paraboloidBuffer);
	}
	/*static*/ ParaboloidBuffer&	ParaboloidBuffer::Allocate(Objects::TurretBase&b)
	{
		return StandardAllocate<ParaboloidBuffer>(b.paraboloidBuffer);
	}
	/*static*/ TowerAttackVisualization&	TowerAttackVisualization::Of(const Tower::AttackTask&t)
	{
		return StandardOf<TowerAttackVisualization>(t.visualization);
	}
	/*static*/ TowerAttackVisualization&	TowerAttackVisualization::Allocate(Tower::AttackTask&t)
	{
		return StandardAllocate<TowerAttackVisualization>(t.visualization);
	}
	/*static*/ StructureHull&	StructureHull::Of(const Objects::Structure&s)
	{
		return StandardOf<StructureHull>(s.shape);
	}

	/*static*/ StructureHull&	StructureHull::Allocate(Objects::Structure&s)
	{
		return StandardAllocate<StructureHull>(s.shape);
	}

	/*static*/ StructureHull&	StructureHull::Of(const Objects::SubOrbital::Plane&s)
	{
		return StandardOf<StructureHull>(s.shape);
	}

	/*static*/ StructureHull&	StructureHull::Allocate(Objects::SubOrbital::Plane&s)
	{
		return StandardAllocate<StructureHull>(s.shape);
	}


	void	CreateComponents()
	{
		MobileEntity::Task::onVisualizationOutdated << TaskAttachment::Recompile;

		ParaboloidBuffer::StaticCreate();
		VisualComponent::FinalizeAll();
		StructureHull::LoadAll();
	}

	void	DestroyComponents()
	{
		ParaboloidBuffer::StaticDestroy();

	}

}
