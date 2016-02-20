#include <global_root.h>
#include "structure.h"
#include "systems.h"
#include "../cast.h"
#include "interface.h"

#ifdef HAS_WINDOW
	#include "visualChoices.h"
	#include "rendering.h"
	#include "visualCommands.h"
	#include "../../client/components/dialog.h"
	#include "interface.h"
#endif

#include "gridAndChoices.h"
#include "prototypes.h"
#include "visualSelection.h"
#include "faction.h"
#include "satellite.h"
#include "vehicle.h"
#include "structureCompound.h"

#ifndef CLIENT
	#include "faction.h"
	#include "../runtimeConfig.h"
#endif

namespace Objects
{


	/**
	All existing structure (building) instances in the world.
	To the server these are all the buildings that exist, to the client these are all
	it currently knows about.
	*/
	Buffer0<PStructure>			structures;
	#ifdef HAS_WINDOW
		/**
		Filter list containing only SkyPorts. Used to faster render certain effects.
		*/
		Buffer0<TLinkReference>	skyLinks;
	#endif

	/**
	List of all known tower types (one for each tower turret type).
	Tower types that are not contained in this list can not be constructed cleanly
	*/
	Buffer0<PTower>				Tower::globalPrototypes;

	/*static*/ Buffer0<Component::VehicleBase*>	VehiclePlant::baseComponents;
	/*static*/ Buffer0<Component::Turret*>			VehiclePlant::turretComponents;
	
	/*static*/ int				NexusPort::ResistenceRounds = 10;


	#ifdef HAS_WINDOW
		/*static*/ Grid::LCellIdI	GatheringPoint::selected;			//!< Gathering point of selected entity (if any, or invalid)
		/*static*/ TMatrix4<>		GatheringPoint::selectedVisual;	//!< Point where the respective gathering point should be visualized at. (same cell; may be different height)

		/*static*/ Geometry			Structure::standardConnector;
		bool						Structure::anyChanged = false;

//		/*static*/ Buffer0<IconCommand>		Connector::commands;
		/*static*/ Buffer0<IconCommand>		Tower::commands;

		/*static*/ DX11::InstructionBlock32	Mine::shape;
		///*static*/ DX11::InstructionBlock32	Connector::shape;
		///*static*/ Geometry					Connector::geometry;

		/*static*/ DX11::InstructionBlock32	Tower::shape;
		/*static*/ Geometry					Tower::geometry;

		DX11::BaseInstructionBlock			Structure::standardCompiledShader, 
											Structure::standardLocalizableShader;


		/*static*/ PVehicle			VehicleStore::deployingVehicle;
		/*static*/ index_t			VehicleStore::storeOffset;
		
		static IconCommand			cmdLeft,cmdRight;

		/*static*/ IconCommand		VehicleStore::cmdLeft,
									VehicleStore::cmdRight;
		/*static*/ Geometry			VehicleStore::interConnector,
									Generator::reactorConnector;

		/*static*/ VehicleConstructionCommand	SkyPort::formerConstructionCommand;
		/*static*/ SatelliteConstructionCommand	SkyPort::skyHubConstructionCommand;


	#endif

	/*static*/ float					Tower::foundationHeight;

	#ifndef CLIENT
		Buffer0<PTower>			towers;	//only used by server ... so far
	#endif



	/*static*/ count_t			Structure::CountOccurrences(const OwnerParameter&o,ConstName type)
	{
		count_t rs = 0;
		for (const auto&m : structures)
			if (m->owner == o && m->GetCTypeName() == type)
				rs++;
			else
			{
//				if (m->GetCTypeName() == type)
	//				ShowMessage(m->owner->GetName() + "!="+o->GetName());
			}
		return rs;
	}




	/*static*/ String				Structure::ToString(placement_t p)
	{
		switch (p)
		{
			case CanPlace:
				return "CanPlace";
			case BadHeight:
				return "Bad location";
			//case LacksCentralEarthFoundation:
			//	return "LacksCentralEarthFoundation";
			//case LacksFullFoundation:
			//	return "LacksFullFoundation";
			case GroundLevelObstruction:
				return "GroundLevelObstruction";
			case UpperLevelObstruction:
				return "UpperLevelObstruction";

		}
		return "Unknown";
	}


	Compound&					Structure::GetCompound() const
	{
		#ifdef ONE_COMPOUND_PER_FACTION
			return owner->compound;
		#else
			return compounds[compoundID];
		#endif

	}



	#ifndef CLIENT

		void						WorkingStructure::SetEnabledOverride(bool b, bool registerChange/*=true*/)
		{
			isEnabled = b;
			if (registerChange)
				RegisterChange(EnableChanged,0);
		
		}

	
		/*virtual override*/ void Tower::Attack(float&attackTime)
		{
			//timeAt = attackTime;


			ASSERT__(attackQueue.IsNotEmpty());

			const auto target = attackQueue.Pop();


			//const Grid::LCellIdI target = task.foci.Pop();
			float3 dir = Grid::low.cells[target.target.stackID].centerVertex.xyz * Grid::ToRadius(target.target.height) - this->GetWorldPosition();
			float3 innerDir;
			Mat::rotate(this->inverse,dir,innerDir);
			TVec3<> t0;
			Grid::HCellIdI t1;
			static Buffer0<TVec4<> > trajectory;

			bool abort = false;
			if (target.autoTarget)
			{
				TurretTrajectory t = TraceTrajectory(owner->currentState,  target.target,trajectory,t0,t1);
				if (t != TurretTrajectory::ReachesTarget)
				{
					abort = true;
				}
			}
			TurretTrajectory t = TurretTrajectory::InvalidTrajectory;
			
			if (!abort)
				t = TraceTrajectory(Grid::actualWorld,  target.target,trajectory,t0,t1);
			if (t != TurretTrajectory::InvalidTrajectory)
			{


				float len = 0;
				for (index_t i = 1; i < trajectory.length(); i++)
					len += Vec::distance(ToWorld(trajectory[i-1]),ToWorld(trajectory[i]));

				const float flightTime =  Times::Attack * len;

				timeAt = std::max(timeAt,attackTime - flightTime - Times::TurretRotation);



				
				float change = 1.f / float(GetAttacksPerRound());
				attackActionLeft -= change;
				RegisterChange(ActionChanged,timeAt);

				{
					//float delta = std::max(fabs(angles.x - turretAngleX), fabs(angles.y - turretAngleY));
					SignalTurretRotation(shared_from_this(),(ToWorld(trajectory[1]) - ToWorld(trajectory.first())).Normalized0(),Times::TurretRotation);
					timeAt += Times::TurretRotation;
				}


				SignalAttack(shared_from_this(),target.target,flightTime,target.autoTarget);

				if (storage.level[turret->ammoType] >= turret->ammoNeedPerShot)
					ChangeStorage(*this,attackTime, turret->ammoType,-turret->ammoNeedPerShot);
				else
				{
					ResourceVector change;
					RefreshCompoundCoverageIfNeeded(timeAt);
					change.level[turret->ammoType] = turret->ammoNeedPerShot;
					GetCompound().Collect(change,timeAt);
					//ASSERT__(change.IsEmpty());
				}
				

				
				timeAt += flightTime;

				if (turret->dealsEMPDamage)
					ExplodeEMP(shared_from_this(),target.target,flightTime,
								t1, t0, turret->explosionRadius, turret->damageLevel,timeAt);
				else
					Explode(shared_from_this(),target.target,flightTime,
							t1, t0, turret->explosionRadius, turret->damageLevel,turret->isDoomsday, timeAt);

				RegisterChange((changeType_t)(TasksChanged),timeAt);
				//stateChangedAt = attackTime;	//problematic
				attackTime = timeAt;
			}
			else
			{
				LogEvent(EventClass::Preset::operationAborted,this,"Unable to plot trajectory to target");
			}



		}

		/*virtual override*/ bool Tower::FillAttackQueue()
		{
			if (this->storage.level[turret->ammoType] < turret->ammoNeedPerShot)
				return false;
			//index_t freeSlots = attackQueue.Count() < t->GetAttacksPerRound() <  t->GetAttacksPerRound() - t->attackQueue.Count() : 0;
			if ((int)attackQueue.Count() < CountPossibleAttacks())
			{
				Grid::LCellIdI target;
				return GetSentryTarget(*this,this->storage.level[turret->ammoType], location,foundationHeight,*this,target) && AttackNext(target,true);
			}
			else
				return false;

		}


	/*virtual*/ void				WorkingStructure::Tick(ResourceVector&v)
		{
			ResourceVector v1, s, m;
			GetResourceVector(v1,s,m);
			v += v1;
		}


		/*virtual override*/ void			Tower::OnConstructionFinished(float atTime)
		{
			Super::OnConstructionFinished(atTime);
			ChangeStorage(*this,atTime,turret->ammoType,GetMaxStorageCapacity().level[turret->ammoType] - storage.level[turret->ammoType]);
			//storage.level[Resource::Munitions] = GetMaxStorageCapacity().level[Resource::Munitions];
			//LogEvent(EventSignificance::Common,this,"Munition storage changed to "+String(storage.level[Resource::Munitions]));

		}

		

		bool Tower::WouldAttack(const PWorldEntity&target, Grid::LCellIdI&outTarget, float entityTime) const
		{
			if (!HasAmmoForShot(entityTime) || empDamageLevel != 0 || IsHibernating())
			{
				//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
				return false;
			}

			static Buffer0<TVec4<> > trajectory;

			TVec3<> iat;
			Grid::HCellIdI ic;


			//float d2 = Grid::GetQuadraticGeometricDistance(location+1,target->location);

			return		//owner->canSee.IsSet(target)
					/*&&	*/turret->CanAttack() 
					&& timeAt <= entityTime 
					&& CountPossibleAttacks() > 0 
					&& ExtCanHit(owner,location,foundationHeight, target->location,outTarget)
					//&& d2 > sqr(turret->explosionRadius)
					&& TraceTrajectory(owner->currentState, target->location,trajectory,iat,ic)==TurretTrajectory::ReachesTarget;
		}




	#endif

	
	#if 0

		/**/						Wall::Wall(const Grid::LCellIdI&at) :Structure(at, WallClass::staticInstance)
		{
			{
				bool rebuild[BaseGrid::NumFamilies] = {};
				UINT32 high = Grid::TagLowCluster(rebuild, this->location);

				Grid::cellStack[high].PutLightStructureI(location.height);

				for (int i = 0; i < BaseGrid::NumFamilies; i++)
					if (rebuild[i])
						Grid::high.family[i].SetOutdated();
			}
		}

		/*virtual override*/ PStructureClass	Wall::GetClass() const
		{
			return WallClass::staticInstance;
		}


		/*virtual override*/ void				Wall::OnNewConnection(const PStructure&to, UINT32 acrossHighCell)
		{
			//Grid::TFamily
			if (location.stackID > to->location.cellID)
			{
				bool rebuild[BaseGrid::NumFamilies] = {};
				Grid::TagLowCluster(rebuild, this->location);
				Grid::TagLowCluster(rebuild, to->location);

				Grid::cellStack[acrossHighCell].PutLightStructureI(location.height);

				for (int i = 0; i < BaseGrid::NumFamilies; i++)
					if (rebuild[i])
						Grid::high.family[i].SetOutdated();
			}
		}

		/*virtual override*/ void				Wall::OnConnectionLost(const PStructure&to, UINT32 acrossHighCell)
		{
			//Grid::TFamily
			if (location.stackID > to->location.cellID)
			{
				bool rebuild[BaseGrid::NumFamilies] = {};
				Grid::TagLowCluster(rebuild, this->location);
				Grid::TagLowCluster(rebuild, to->location);

				Grid::cellStack[acrossHighCell].ClearLightStructureI(location.height);

				for (int i = 0; i < BaseGrid::NumFamilies; i++)
					if (rebuild[i])
						Grid::high.family[i].SetOutdated();
			}
		}



		/*virtual override*/ bool	WallClass::CanPlace(const Grid::LCellIdI&location)
		{
			const auto&stack = Grid::cellStack[Grid::low.grid.GetCell(location.cellID).higherEquivalent];
			return NoStructureHere(location) && stack.GetCellI(location.height - 1).IsSolid() && !stack.GetCellI(location.height).IsOccupied();
		}
		/*virtual override*/ PStructure			WallClass::CreateInstance(const Grid::LCellIdI&location)
		{
			PStructure rs(new Wall(location));
			return rs;
		}
		/*virtual override*/ void	WallClass::Setup()
		{
			MakeBox(connectorMesh, Box<>(-0.2f, 0.f, 0.f, 0.2f, 1.f, 1.f));
			MakeCylinder(centralMesh, float3(), 0.2f, 1.f);
		}



		/*virtual override*/ bool Wall::ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)
		{
			if (other->GetClass() != GetClass())
				return false;
			if (other->location.height != this->location.height)
				return false;
			return !Grid::cellStack[acrossHighCell].GetCellI(location.height).IsSolid();
		}



		/*virtual override*/ void				Wall::Recompile()
		{
			changed = false;

			LogEvent("Recompiling " + GetNiceName());
			localMesh.ClearCompletely();











			{
				const Geometry&source = WallClass::staticInstance->connectorMesh;

				for (index_t lod = 0; lod < source.CountLODs(); lod++)
				{
					const Geometry::LOD&data = source.GetLOD(lod);
					//const Vertex*vertexData = data.vertices.pointer();
					localMesh.AppendAndActivateNewLOD();
					const count_t numVertices = data.vertices.Count();
					foreach(this->connectedTo, c)
					{
						if ((*c)->location.stackID > location.cellID)
							continue;	//only one direction
						localMesh.SetVertexOffsetToCurrent();
						const float3&b0 = Grid::low.grid.GetCell(this->location.cellID).center;
						const float3&b1 = Grid::low.grid.GetCell((*c)->location.cellID).center;
						const float3 d = b1 - b0;
						const float scale = d.Length();
						const float3 d1 = d / scale;
						const float r0 = Grid::ToRadius(location.height);
						const float r1 = Grid::ToRadius(location.height+1);

						const float3 x = -(b0 | d1).Normalized() * (r1 - r0);
						for (index_t i = 0; i < numVertices; i++)
						{
							const Vertex&vtx = data.vertices[i];
							const TVec3<>&position = vtx.vertex;
							float r = r0 + (r1 - r0) * position.z;
							TVec3<> np = b0 + d * position.y;
							Vec::normalize(np);
							np *= r;
							np += x * position.x;
							localMesh.MakeVertex(np, float3(), vtx.color);	//dont care about uv for now
						}
						localMesh.AppendTriangleIndices(data.indices);
					}
					localMesh.ComputeNormals();
				}
			}
		//	if (connectedTo.Count() != 1)
			{
				const Geometry&source = WallClass::staticInstance->centralMesh;

				for (index_t lod = 0; lod < source.CountLODs(); lod++)
				{
					const Geometry::LOD&data = source.GetLOD(lod);
					//const Vertex*vertexData = data.vertices.pointer();
					localMesh.SetLOD(lod);
					const count_t numVertices = data.vertices.Count();

					localMesh.SetVertexOffsetToCurrent();
					TMatrix4<> system = Matrix<>::eye4;
					const float r0 = Grid::ToRadius(this->location.height);
					Mat::makeAxisSystem(Grid::low.grid.GetCell(this->location.cellID).center*r0, Grid::low.grid.GetCell(this->location.cellID).center, 2, system);
					const float scale = Grid::ToRadius(this->location.height + 1) - r0;
					system.x.xyz *= scale;
					system.y.xyz *= scale;
					system.z.xyz *= scale;
					for (index_t i = 0; i < numVertices; i++)
					{
						const Vertex&vtx = data.vertices[i];
						const TVec3<>&position = vtx.vertex;
						TVec3<> np,n;
						//np = system.w.xyz + system.x.xyz * position.x + system.y.xyz * position.y;
						//Vec::normalize(np);
						//system.z.xyz = np * scale;
						//np *= r0 + scale * position.z;
					
						Mat::transform(system, vtx.vertex, np);
						Mat::rotate(system, vtx.normal, n);
						n /= scale;



					//	Vec::normalize(np);
						//np *= Grid::ToRadiusf(float(location.height) + position.z);

						localMesh.MakeVertex(np, n, vtx.color);	//dont care about uv for now
					}
					localMesh.AppendTriangleIndices(data.indices);
				
				}
			}


			if (shape.shader.IsEmpty())
				(DX11::BaseInstructionBlock&)shape = stdVehicleShader;
			shape.vbo.CreateFromStructs(localMesh.GetLOD(0).vertices);
			shape.ibo.Create(localMesh.GetLOD(0).indices);
		}

	#endif /*0*/
	

	Structure::Structure(const OwnerParameter&owner, const Grid::LCellIdI&at, bool buildSystem, float time) :Super(owner,at)
		#ifdef HAS_WINDOW
			, localizableShape(nullptr)
		#endif
	{
		if (buildSystem)	//makesystem createsystem
		{
			UpdateAutomaticSystem();
		}
	}

	void			Structure::UpdateAutomaticSystem()
	{
		BuildStructureSystem(location,system);

		Mat::invert(system,inverse);
		#ifdef HAS_WINDOW
			ambientSound.SetPosition(GetWorldPosition());
		#endif
	}


	#ifdef HAS_WINDOW
	
		/*virtual override*/ void				Structure::Recompile()
		{
			changed = false;

			LogEvent(EventClass::Preset::common, this, "Recompiling geometry");
			localMesh.ClearCompletely();

			{
				for (const auto&c : this->connectedTo)
				//foreach(this->connectedTo, c)
				{
					if (c->location.stackID > location.stackID)
						continue;	//only one direction

					const Geometry*source = GetSourceForConnection(c);
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

						const TVec3<>&center0 = this->GetWorldPosition(),
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
						const float l0 = GetLowLighting(this->location + this->GetHeight()-1);
						const float l1 = GetLowLighting(c->location + this->GetHeight()-1);
						const float ld = l1 - l0;
						//const float3 x = -(b0 | d1).Normalized();// * (r1 - r0);
						const float3 x = -(float3::Reinterpret(center0) | centerDelta).Normalized();// * (r1 - r0);
						const float horzScaleBase = Vec::length(this->system.x.xyz);
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
					(DX11::BaseInstructionBlock&)compiledShape = standardCompiledShader;
				compiledShape.vbo.CreateFromStructs(localMesh.GetLOD(0).vertices);
				compiledShape.ibo.Create(localMesh.GetLOD(0).indices);
			}
			else
			{
				compiledShape.vbo.Clear();
				compiledShape.ibo.Clear();
			}
		}
	#endif





	void Structure::StaticSetup()
	{
		#ifdef HAS_WINDOW
			standardConnector.LoadFromObj("objects/standardConnector.obj");
		#endif
	}
	
	void	Structure::DisconnectFromNeighborhood(float atTime)
	{
		foreach (this->connectedTo,conn)
		{
			(*conn)->SignalLostNeighbor(To<Structure>::Cast(shared_from_this()),atTime);
		}
		this->connectedTo.Clear();
		#ifdef HAS_WINDOW
			this->changed = anyChanged = true;
		#endif
	}


	void Structure::OnConstructionFinished(float atTime)
	{
		Super::OnConstructionFinished(atTime);


		CommonRegisterStructure(location,GetBroadHeight(),GetHeight(),HasFoundation(),atTime);

		TStructureNeighborhood	neighborhood;
		GetNeighborhood(owner,ConnectsToDifferentOwnership(), location, neighborhood);

		#ifdef HYBRID_GRID
			const UINT32 high = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			const auto&ng = Grid::high.grid.GetCell(high).neighborhood;
		#endif

		{
		//String dbgStr;
		//foreach (neighborhood,n)
		//	dbgStr += ", "+ n->get()->GetName();
		//LogEvent(EventSignificance::Debug,nullptr, "Neighborhood of "+s->GetName()+"[owner "+s->owner.ToString()+"]@"+s->location.ToString()+": "+dbgStr);

		//Structure&_s = *s;
		}

		PStructure _this = To<Structure>::Cast(shared_from_this());
		foreach(neighborhood, n)
		{
			//foreach(connectedTo, c)
			ASSERT__(abs(location.height - (*n)->location.height)<=1);

			const PStructure&c = *n;
			#ifdef HYBRID_GRID
				const UINT32 high2 = Grid::low.grid.GetCell((c)->location.stackID).higherEquivalent;
				const auto&n2 = Grid::high.grid.GetCell(high2).neighborhood;
				UINT32 between = BaseGrid::RequireIntersection(ng, n2);
			#else
				UINT32 between = 0;
			#endif
			if (ConnectionPossibleTo(c, between) && c->ConnectionPossibleTo(_this, between))
			{
				connectedTo << c;
				c->connectedTo << _this;
				#ifdef HAS_WINDOW
					changed = c->changed = Structure::anyChanged = true;
				#endif
				c->OnNewConnection(_this, between, atTime);
				if (!c->connectedTo.Contains(_this))
				{
					//this can happen when placing new structures and c is a nexus port that changes ownership as a result, and thus reconnects to all existing structures which this is not yet a part of
					DBG_ASSERT__(!connectedTo.Contains(c));
					connectedTo << c;
					c->connectedTo << _this;
					c->OnNewConnection(_this, between, atTime);
					ASSERT__(c->connectedTo.Contains(_this));
				}
				_this->OnNewConnection(c, between, atTime);
				LogEvent(EventClass::Preset::common,"Connection established between "+GetName()+" and "+c->GetName());
				#ifdef HAS_WINDOW
					if (c->changed)
						c->Recompile();
				#endif

			}
		}
		#ifdef HAS_WINDOW
			Recompile();
		#endif
	}




	/*virtual*/ void Structure::OnDestroy(float atTime)
	{
		Super::OnDestroy(atTime);
		LogEvent(EventClass::Preset::debug,this,"Unregistering...");
		CommonUnregisterStructure(location,GetBroadHeight(),GetHeight(),HasFoundation(),atTime);

		LogEvent(EventClass::Preset::debug,this,"Disconnecting...");
		DisconnectFromNeighborhood(atTime);

		#ifdef HAS_WINDOW
			this->compiledShape = Engine::DX11::InstructionBlock32();
			this->localizableShape = nullptr;
			this->localMesh.ClearCompletely();
			this->changed = false;
		#endif
	}


	/*virtual override*/	void			Mine::PrototypeSetup()
	{
		Super::PrototypeSetup();
		#ifdef HAS_WINDOW
			iconCommands << IconCommand(int2(5,1), "Restore Core Link",[](const PWorldEntity&s)
			{
				PMine mine = To<Mine>::DynamicCast(s);
				if (mine)
				{
					SignalRestoreCoreLink(mine->id);
				}
			},
			[](const PWorldEntity&s, bool controlEnabled)
			{
				if (!controlEnabled)
					return BaseCommand::Flags::None;
				PMine mine = To<Mine>::DynamicCast(s);
				return mine && !mine->IsLinkedToCore() ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
			},
			[](const PWorldEntity&, int&mineralCostOut, int&timeCostOut){mineralCostOut = 0; timeCostOut = 0;}
			);
		#endif
	}

	#ifdef HAS_WINDOW
		/*virtual override*/ void				Tower::VisitAllCommands(FCommandVisitor visitor)
		{
			foreach (commands,cmd)
				if (turret->CanAttack() || cmd->name!="Attack")
					visitor(*cmd);
		}

	#endif

	bool	Tower::setup = false;
	/*virtual override*/	void			Tower::PrototypeSetup()
	{
		if (setup)
			return;
		setup = true;
		Super::PrototypeSetup();
		//turretComponents << artillery;
		//turretComponents << Armored::component;
		//turretComponents << radar;
		//turretComponents << MunitionTransporter::component;

		#ifdef HAS_WINDOW
			geometry.Clear();
			commands << IconCommand(int2(4,0), "Abort", &VehicleCmd::AbortOperation, &CommonCmd::CommonButtonCheck
						,NoCosts
			);
			commands << IconCommand(int2(4,1), "Undo", &VehicleCmd::UndoOperation, &CommonCmd::CommonButtonCheck
						,NoCosts
			);
			commands << IconCommand(int2(5,1), "Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>
						,NoCosts
			);
			commands << IconCommand(int2(5,0), "Demolish",&SignalRemoval,&CommonCmd::CommonButtonCheck
						,NoCosts
			);
		#endif

		static const float staticHeight = 1.005f;

		#ifdef HAS_WINDOW
			String name = GetTypeName();
			name.set(0, Template::tolower(name.firstChar()));
			name.replace(' ',"");
			geometry.LoadFromObj("objects/tower.obj");

			if (shape.shader.IsEmpty())
				(DX11::BaseInstructionBlock&)shape = standardLocalizableShader;
			shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
			shape.ibo.Create(geometry.GetLOD(0).indices);
			foundationHeight = geometry.GetBoundingBox().z.max;

			ASSERT2__(similar(staticHeight,foundationHeight),staticHeight,foundationHeight);
		#else
			foundationHeight = staticHeight;
		#endif

	}

	#ifndef CLIENT
		/*virtual override*/ void			Tower::ProcessLeftovers(ResourceVector&v, index_t iteration, float atTime)
		{
			ResourceVector maxStorage = GetMaxStorageCapacity();
			index_t targetIteration = 1;
			#ifndef NO_MUNITION
				if (storage.level[Resource::Munitions] < maxStorage.level[Resource::Munitions] / 4)
					targetIteration = 0;
			#endif
			if (iteration != targetIteration && iteration != FinalLeftoverIteration)
				return;
			for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
			{
				if (v.level[i] == 0)
					continue;
				if (v.level[i] < 0 )
				{
					if (storage.level[i] > 0)
					{
						int delta = std::min(-v.level[i], storage.level[i]);

						ChangeStorage(*this,atTime,Resource::Reinterpret(i),-delta);

						v.level[i] += delta;
					}
				}
				else
				{
					int capacity = maxStorage.level[i] - storage.level[i];
					if (capacity > 0)
					{
						int delta = std::min(v.level[i], capacity);
						v.level[i] -= delta;

						ChangeStorage(*this,atTime,Resource::Reinterpret(i),delta);
					}	
				}
			}
		}

		bool					Tower::AttackNext(const Grid::LCellIdI&target, bool autoTarget)
		{
			if (attackQueue.Count() >= MaxStructureTasks)
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(MaxStructureTasks)+") reached");
				return false;
			}
			if (!turret->CanAttack())
			{
				LogEvent(EventClass::Preset::commandRejected,this,"This tower cannot attack");
				return false;
			}

			if (target != location+1)
			{
				//Grid::LCellIdI pointOfOrigin = location+1;


				//float targetDistance = Grid::GetGeometricDistance(pointOfOrigin,target);

				if (!CanHit(target))
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
					return false;
				}
			}

			AttackTask&cmd = attackQueue.Push();
//			cmd.id.Generate();
			cmd.target = target;
			cmd.autoTarget = autoTarget;
			SignalChange(TasksChanged,0);
			return true;
		}


		/*virtual override*/ bool			Tower::DirectTo(CursorMode mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
		
			if (mode != Objects::CursorMode::AttackMode || !leftClick)
				return false;
			for (const auto&destination : waypoints)
			{
				if (!AttackNext(destination,false))
					return false;
				LogEvent(EventSignificance::Common,this,"Issued tower attack to "+destination.ToString());
			}
			return true;
		}

		bool					Tower::ClearTasks()
		{
			if (attackQueue.IsEmpty())
				return false;
			attackQueue.Clear();
			SignalChange(TasksChanged,0);
			return true;
		}

		bool					Tower::DropLastTask()
		{
			if (attackQueue.IsEmpty())
				return false;
			attackQueue.EraseBack();
			SignalChange(TasksChanged,0);
			return true;
		}
	#else

		#ifdef HAS_WINDOW
			/*virtual override*/ bool			Tower::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
			{
				TVec3<> b,d;//,test;
				Mat::transform(inverse,_b,b);
				Mat::rotate(inverse,_d,d);
				return geometry.TestIntersection(b,d,distance) | turret->TestIntersection(foundationHeight, turretAngleX,turretAngleY,b,d,distance);	//binary or - must evaluate both
			}
		#endif
	#endif


	bool Mine::LinkToCore(float atTime)
	{
		#ifdef CLIENT
			return isLinkedToCore;
		#else
			this->naturalResources = NaturalResourceVector();
			bool success = true;
			{
				#ifdef HYBRID_GRID
					const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
				#else
					const UINT32 higher = location.cellID;
				#endif
				Grid::CellStack&stack = Grid::actualWorld.cellStack[higher];

				UINT layer = Grid::CellStack::ToLayer(location.height);
				UINT i = 0;
				if (Grid::CellStack::bottom.content == Grid::Content::Clear)
					while (stack.volumeCell[i].content == Grid::Content::Clear && i < layer)
						i++;	//skip air if no ground
				if (i == layer)
					return false;	//this is REALLY weird, because the foundation would be missing

				for (UINT j = layer-1; j >= i && j < layer; j--)
				{
					int h = Grid::CellStack::ToHeight(j);
					if ((j == layer-1 || !stack.GetCellU(j).structureCounter) && !GetVehicle(Grid::LCellIdI(location.stackID,h)))
						stack.LocalOverwrite(h, Grid::Content::Foundation);
					else
					{
						success = false;
						break;
					}
				}
				Grid::actualWorld.SignalChangeUpTo(location-1);
	//			stack.UpdateLighting();
			}
			InvalidateTerrain(location,atTime);

			if (success)
			{
				naturalResources.SampleWorld(location.stackID);
			}
			if (success)
				RegisterChange(WorldEntity::CanEnableChanged,atTime);
			return success;
		#endif
	}



	bool	Tower::HasAmmoForShot(float atTime) const
	{
		#ifndef CLIENT
			RefreshCompoundCoverageIfNeeded(atTime);
		#endif
		return GetCompound().total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	}

	bool	Tower::HasAmmoForShot() const
	{
		return GetCompound().total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
		//compoundID < compounds.Count() && compounds[compoundID].total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	}




	


	bool	Mine::IsLinkedToCore()	const
	{
		#ifdef CLIENT
			return isLinkedToCore;
		#else
			#ifdef HYBRID_GRID
				const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			#else
				const UINT32 higher = location.cellID;
			#endif
			const Grid::CellStack&stack = Grid::actualWorld.cellStack[higher];
			UINT layer = Grid::CellStack::ToLayer(location.height);
			UINT i = 0;
			if (Grid::CellStack::bottom.content == Grid::Content::Clear)
				while (stack.volumeCell[i].content == Grid::Content::Clear && i < layer)
					i++;	//skip air if no ground
			if (i == layer)
				return false;	//this is REALLY weird, because the foundation would be missing

			for (; i < layer; i++)
			{
				if (stack.GetCellU(i).content != Grid::Content::Foundation)
					return false;
			}
			return true;
		#endif
	}

	#ifdef CLIENT
		void	Mine::SetLinkedToCore(bool b, bool controlEnabled)
		{
			if (isLinkedToCore == b)
				return;
			isLinkedToCore = b;
			#ifdef HAS_WINDOW
				if (selectedEntity.get() == this)
					UpdateChoices(controlEnabled);
			#endif
		}
	#endif






	///*virtual override*/	void			Connector::PrototypeSetup()
	//{

	//	#ifdef HAS_WINDOW
	//		commands << IconCommand(int2(4,0), "Demolish",&SignalRemoval,&CommonCmd::MostlyEnabled<PWorldEntity const&>,&CommonCmd::AlwaysFalse<PWorldEntity const&>, NoCosts);
	//		geometry.Clear();

	//		geometry.LoadFromObj("objects/connector.obj");

	//		if (shape.shader.IsEmpty())
	//			(DX11::BaseInstructionBlock&)shape = standardLocalizableShader;
	//		shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
	//		shape.ibo.Create(geometry.GetLOD(0).indices);
	//	#endif
	//}


	Connector::Connector(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location, true, atTime)
	{}

	/*virtual override*/ void				Connector::OnDestroy(float atTime)
	{
		Super::OnDestroy(atTime);
	}










	/*static*/ void Structure::ChangeStructCount(const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, int changeDirection, float atTime)
	{
		LogEvent(EventSignificance::Debug,nullptr,"ChangeStructCount("+location.ToString()+", broadHeight="+String(broadHeight)+", height="+String(totalHeight)+", foundation="+String(hasFoundation)+", dir="+String(changeDirection)+")");

		{
			const int lowEdge = hasFoundation ? -1 : 0;
			#ifdef HYBRID_GRID
				const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(higher).neighborhood;
				for (int j = 0; j < n.numNeighbors; j++)
				{
					Grid::CellStack&stack = Grid::actualWorld.cellStack[n.neighborID[j]];
					for (int h = lowEdge; h < broadHeight; h++)
						stack.ChangeStructureCounterI(location.height+h,changeDirection);
				}
				Grid::CellStack&stack = Grid::actualWorld.cellStack[higher];
			#else
				Grid::CellStack&stack = Grid::actualWorld.cellStack[location.stackID];
			#endif
			for (int h = lowEdge; h < broadHeight; h++)
				stack.ChangeStructureCounterI(location.height+h,2*changeDirection);
			for (int h = broadHeight; h < totalHeight; h++)
				stack.ChangeStructureCounterI(location.height+h,changeDirection);
		}

		InvalidateTerrain(location,atTime);
	}

	/*static*/ void	Structure::CommonRegisterStructure(const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, float atTime)
	{
		ChangeStructCount(location,broadHeight, totalHeight,hasFoundation,+1,atTime);
	}
	/*static*/ void	Structure::CommonUnregisterStructure(const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, float atTime)
	{
		ChangeStructCount(location,broadHeight, totalHeight,hasFoundation,-1,atTime);
	}



	/*static*/	Structure::placement_t	Structure::CommonCheckStructurePlacement(const Grid::LCellIdI&location, int broadHeight, int totalHeight)
	{
		if (location.height == badHeight)
			return BadHeight;
		#ifdef HYBRID_GRID
			const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			{
				const auto&n = Grid::high.grid.GetCell(higher).neighborhood;
				for (int j = 0; j < n.numNeighbors; j++)
				{
					const Grid::CellStack&stack = Grid::actualWorld.cellStack[n.neighborID[j]];
	//					if (stack.GetCellI(location.height - 1).content != Grid::CellStack::Cell::Content::Foundation)
	//					return LacksFullFoundation;
					if (stack.GetCellI(location.height).content != Grid::CellStack::Cell::Content::Clear)
						return GroundLevelObstruction;
					for (int h = 1; h < broadHeight; h++)
						if (stack.GetCellI(location.height + h).content != Grid::CellStack::Cell::Content::Clear)
							return UpperLevelObstruction;
				}
			}
			const Grid::CellStack&stack = Grid::actualWorld.cellStack[higher];
		#else
			const Grid::CellStack&stack = Grid::actualWorld.cellStack[location.stackID];
		#endif
	//	if (stack.GetCellI(location.height - 1).content != Grid::CellStack::Cell::Content::Foundation)
		//	return LacksFullFoundation;
		if (stack.GetCellI(location.height).content != Grid::CellStack::Cell::Content::Clear)
			return GroundLevelObstruction;
		for (int h = 1; h < totalHeight; h++)
			if (stack.GetCellI(location.height + h).content != Grid::CellStack::Cell::Content::Clear)
				return UpperLevelObstruction;



		return CanPlace;
	}

	/*virtual override*/ void				Generator::OnNewConnection(const PStructure&other, UINT32 acrossHighCell, float atTime)
	{
		Super::OnNewConnection(other,acrossHighCell,atTime);
		if (connectedToReactor.expired() && other->location.height == location.height)
		{
			PReactorCore reactor = To<ReactorCore>::DynamicCast(other);
			if (reactor)
			{
				connectedToReactor = reactor;
				#ifdef HAS_WINDOW
					changed = anyChanged = true;
				#endif
			}
		}
	}

	/*virtual override*/ void				Generator::OnConnectionLost(const PStructure&s, UINT32 acrossHighCell, float atTime)
	{
		Super::OnConnectionLost(s,acrossHighCell,atTime);
		PReactorCore core = connectedToReactor.lock();
		if (core == s || !core)
		{
			foreach (connectedTo,conn)
			{
				core = To<ReactorCore>::DynamicCast(*conn);
				if (core && core->location.height == location.height)
				{
					connectedToReactor = core;
					#ifdef HAS_WINDOW
						changed = anyChanged = true;
					#endif
					return;
				}
			}
		}
	}


	VehicleStore::VehicleStore(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location, true,atTime)
	{}


	/*virtual override*/ bool						VehicleStore::CanStoreType(const MobileEntity&e) const 
	{
		return e.IsVehicle();
	}
	/*virtual override*/ bool				VehicleStore::HasStoredLocally(const PMobileEntity&e) const 
	{
		return e->IsVehicle() && VehicleStore::HasStoredLocally(To<Vehicle>::Cast(e));
	}

	/*virtual override*/ void				VehicleStore::StoreLocallyFromImport(const PMobileEntity&e)
	{
		ASSERT__(e);
		ASSERT1__(e->IsVehicle(),e->GetName());
		float t = 0.f;
		ASSERT__(VehicleStore::StoreLocally(To<Vehicle>::Cast(e),t,false));
	}


	/*virtual override*/ PHangarStructure			VehicleStore::CastToDestinationForType(const MobileEntity&e)
	{
		if (CanStoreType(e))
		{
			return To<VehicleStore>::Cast(shared_from_this());
		}
		return PHangarStructure();
	}







	/*virtual override*/ void				VehicleStore::OnNewConnection(const PStructure&s, UINT32 acrossHighCell, float atTime)
	{
		Super::OnNewConnection(s,acrossHighCell,atTime);

		PVehicleStore other = To<VehicleStore>::DynamicCast(s);
		if (other && other->location.height == location.height)
		{
			sneighbors.Append(other);

			if (other->park)
				MergePark(other->park);
			else
			{
				if (park)
				{
					park->members.Append(other);
					other->park = park;
				}
				else
				{
					park.reset(new Park());
					park->members.Append(To<VehicleStore>::Cast(shared_from_this()));
					park->members.Append(other);
					other->park = park;
				}
			}

		}


	}
	/*virtual override*/ void				VehicleStore::OnConnectionLost(const PStructure&s, UINT32 acrossHighCell, float atTime)
	{
		Super::OnConnectionLost(s,acrossHighCell,atTime);

		PVehicleStore other = To<VehicleStore>::DynamicCast(s);
		if (other)
		{
			foreach (sneighbors,_s)
			{
				if (_s->lock() == other)
				{
					sneighbors.erase(_s);
					break;
				}
			}
		}
	}

	void						VehicleStore::PropagatePark(const PPark&c)
	{
		if (park == c)
			return;
		MergePark(c);
		for (const auto&n : sneighbors)
			n.lock()->PropagatePark(c);
	}


	void						VehicleStore::MergePark(const PPark&other)
	{
		if (other)
		{
			if (this->park)
			{
				if (park == other)
					return;
				//compound->members.Append(other);
				for (const auto&_store : other->members)
				{
					PVehicleStore store = _store.lock();
					DBG_ASSERT__(store);
					if (store)
					{
						DBG_ASSERT__(!park->Contains(store));
						DBG_ASSERT__(store->park != park);
						park->members.Append(store);
						store->park = park;
					}
				}
				park->vehiclesStored += other->vehiclesStored;
				other->vehiclesStored = 0;
			}
			else
			{
				park = other;
				park->members.Append(To<VehicleStore>::Cast(shared_from_this()));
				park->vehiclesStored += localStore.Count();
			}
		}
	}

	bool	VehicleStore::StoreLocally(const PVehicle&v, float&atTime, bool setHealthToFullIfNew/*=true*/)
	{
		if (LocalIsFull() || v->owner != owner || v->health <= 0)
			return false;
		#ifndef CLIENT
			atTime = std::max(atTime,v->timeAt) + 0.05f;
			v->timeAt = atTime;
		#endif
		if (v->isInserted)
		{
			Remove(v,RemovalType::EnteredStorage,atTime,false,!v->isInsertedIntoGrid,true);
			#ifndef NO_HIBERNATION
				v->Hibernate(false,atTime);
			#endif
		}
		else
		{
			if (!GetEntity(v->id))
			{
				RegisterNewOffGrid(v,atTime,setHealthToFullIfNew);
			}
		}
		LogEvent(EventSignificance::Common,v.get()," Now stored in "+GetNiceName());
		v->storedIn = To<VehicleStore>::Cast(shared_from_this());
		//v->location = location;
		//#ifndef CLIENT
		//	v->SignalChange(WorldEntity::PositionChanged,atTime);
		//#endif
		this->localStore.Append().vehicle = v;

		#ifdef HAS_WINDOW
			WVehicle w = v;
			this->localStore.last().deployCommand = DeployVehicleCommand(int2(),v,
				[w](const PWorldEntity&storeEntity)
				{
					//focus = To<VehicleStore>::DynamicCast(storeEntity).get();
					if (deployingVehicle)
						deployingVehicle.reset();
					else
						deployingVehicle = w.lock();
				},
				[w](const PWorldEntity&storeEntity, bool controlEnabled)
				{
					BaseCommand::Flags rs = CommonCmd::CommonButtonCheck(storeEntity,controlEnabled);
					PVehicleStore s = To<VehicleStore>::DynamicCast(storeEntity);
					if (deployingVehicle && s == selectedEntity && deployingVehicle == w.lock() && deployingVehicle->health > 0)
						rs |= BaseCommand::Flags::IsPermaActive;
					return rs;
				}
				);
			this->localStore.last().deployCommand.RasterizeImage();
		#endif

		if (park)
			park->vehiclesStored ++;
		

		#ifndef CLIENT
			//RegisterChange(StorageChanged,atTime);	might be irrelevant. let's see
			v->RegisterChange(StorageChanged,atTime);
		#endif
		#ifdef HAS_WINDOW
			VehicleStore::UpdateIconLocations();
		#endif

		ASSERT1__(!v->isInsertedIntoGrid,v->GetNiceName());

		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif

		return true;
	}

	bool						VehicleStore::HasStoredLocally(const PVehicle&v) const
	{
		for (const auto&s : localStore)
			if (s.vehicle == v)
				return true;
		return false;
	}



	#ifdef HAS_WINDOW
		/*virtual override*/ void			VehicleStore::PrototypeLoadGeometry()
		{
			Super::PrototypeLoadGeometry();

			interConnector.LoadFromObj("objects/vehicleStoreConnector.obj");
		}

		/*virtual override*/ void			Generator::PrototypeLoadGeometry()
		{
			Super::PrototypeLoadGeometry();

			reactorConnector.LoadFromObj("objects/reactorCoreConnector.obj");
		}

	#endif


	/*virtual override*/ void				VehicleStore::PrototypeSetup()
	{
		Super::PrototypeSetup();

		#ifdef HAS_WINDOW



			cmdLeft = IconCommand(int2(4,1),"Left",
				[](const PWorldEntity&store)
				{
					PVehicleStore s = To<VehicleStore>::DynamicCast(store);
					if (storeOffset > 0)
					{
						if (storeOffset > MaxVisibleStoreItems)
							storeOffset -= MaxVisibleStoreItems;
						else
							storeOffset = 0;
						if (s)
							s->UpdateIconLocations();
						RefreshCommands(true);
					}
				},
				[](const PWorldEntity&, bool controlEnabled)
				{
					return storeOffset > 0 && controlEnabled
						 ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
				},
				NoCosts);

			cmdRight = IconCommand(int2(),"Right",
				[](const PWorldEntity&store)
				{
					PVehicleStore s = To<VehicleStore>::DynamicCast(store);
					if (!s)
						return;
					if (s->CountVehiclesInPark() <= storeOffset)
						storeOffset = 0;
					else
					{
						count_t remaining = s->CountVehiclesInPark() - (storeOffset);
						if (remaining > MaxVisibleStoreItems)
							storeOffset += MaxVisibleStoreItems;
						//else
							//storeOffset = s->CountVehiclesInPark() - MaxVehiclesPerStore;
					}
					s->UpdateIconLocations();
					RefreshCommands(true);
				},
				[](const PWorldEntity&store, bool controlEnabled)
				{
					PVehicleStore s = To<VehicleStore>::DynamicCast(store);
					return s && s->CountVehiclesInPark() > storeOffset + MaxVisibleStoreItems
					  && controlEnabled
					   ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
				},
				NoCosts);
			cmdRight.coords = cmdLeft.coords + int2(MaxVisibleStoreItems+1,0);

//			bool b = true;
		#endif

	}


	#ifdef HAS_WINDOW

		/*static*/ void				VehicleStore::StaticSignalDeployment()
		{
			PVehicleStore st = To<VehicleStore>::DynamicCast(selectedEntity);
			if (st)
				st->SignalDeployment();
		}

		

		void					VehicleStore::SignalDeployment()
		{
			deployingVehicle.reset();	//might be unset before this method is called, too
			UpdateIconLocations();
		}

		/*virtual override*/ void		VehicleStore::VisitAllCommands(FCommandVisitor visitor)
		{
			Super::VisitAllCommands(visitor);
			if (!park)
			{
				foreach (localStore,v)
					visitor(v->deployCommand);
				return;
			}
			if (storeOffset > 0)
				visitor(cmdLeft);
			index_t at = 0;
			foreach (park->members,m)
			{
				PVehicleStore s = m->lock();
				DBG_ASSERT__(s);
				if (s)
				{
					foreach (s->localStore,s2)
					{
						if (at >= storeOffset)
						{
							s2->deployCommand.coords = cmdLeft.coords + int2(at - storeOffset+1 ,0);
							visitor(s2->deployCommand);
						}

						at++;
						if (at >= storeOffset + MaxVisibleStoreItems)
							break;
					}
				}
				if (at >= storeOffset + MaxVisibleStoreItems)
					break;
			}
			if (this->park->vehiclesStored - storeOffset > MaxVisibleStoreItems)
				visitor(cmdRight);
		}

	#endif


	void	VehicleStore::LocalCheckout(const PVehicle&v, const Grid::LCellIdI&targetLocation, float&atTime)
	{
		ASSERT1__(v,GetName());

		#ifndef CLIENT
		{
			String error;
			if (!owner->CanSpawnFromStorage(v,error))
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Failed to check out "+v->GetNiceName()+": "+error);
				return;
			}
		}
		#endif

		ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif
		ASSERT3__(v->storedIn.lock().get() == this, v->GetName(),v->storedIn.lock()->ToEntity().GetName(),GetName());
		ASSERT2__(RemoveStored(v),v->GetName(),GetName());
		LogEvent(EventSignificance::Debug,this,"Checking out "+v->GetName()+" to "+targetLocation.ToString());
		#ifndef CLIENT
			
		


			MobilePlacementResult rs = v->PlaceExactlyAt(targetLocation,0.f,0.f,false,false,true);
			if (rs != MobilePlacementResult::PlacementOkay && rs != MobilePlacementResult::PlacementOkayFellDown)
			{
				if (v->health <= 0)
				{
					LogEvent(EventSignificance::Debug,this,v->GetName()+" did not survive checkout to "+targetLocation.ToString());
				}
				else
				{
					LogEvent(EventClass::Preset::fatal,this,"Checkout of "+v->GetName()+" failed unexpectedly: "+String(rs));
					FATAL__("Checkout failed");
				}
			}
			else
			{
				v->RegisterChange(PositionChanged,atTime);
				ASSERT3__(v->location.stackID == targetLocation.stackID,GetName(),v->GetName(),targetLocation.ToString());
			}
		#else
			v->location = targetLocation;
		#endif
		if (v->health > 0)
		{
			InsertConstructed(v,atTime);
			#ifndef CLIENT
				v->SignalDeployment();
			#endif
		}
		else
			v->storedIn.reset();
		ASSERT__(v->storedIn.expired());
		if (park)
			park->vehiclesStored --;
		#ifdef HAS_WINDOW
			UpdateIconLocations();
		#endif
		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif
	}

	#ifdef HAS_WINDOW
		void	VehicleStore::UpdateIconLocations()
		{
			count_t range = CountVehiclesInPark();
			if (range > MaxVisibleStoreItems)
			{
				while (storeOffset > 0 && storeOffset >= range)
					storeOffset -= MaxVisibleStoreItems;
			}
			else
			{
				storeOffset = 0;
			}
			index_t current = 0;
			if (park)
			{
				foreach (park->members, m)
				{
					PVehicleStore s = m->lock();
					DBG_ASSERT__(s);
					if (s)
					{
						foreach (s->localStore, st)
						{
							if (current >= storeOffset)
								st->deployCommand.coords = cmdLeft.coords + int2(1 + current,0);
							current ++;
						}
					}
				}
			}
			else
			{
				foreach (localStore, st)
				{
					if (current >= storeOffset)
						st->deployCommand.coords = cmdLeft.coords + int2(1 + current,0);
					current ++;
				}
			}
		}
	#endif

	#ifdef CLIENT
		void	VehicleStore::Lose(const PVehicle&v)
		{
			ASSERT3__(v->storedIn.lock().get() == this,v->GetName(),v->storedIn.lock()->ToEntity().GetName(),GetName());
			v->storedIn.reset();
			ASSERT2__(RemoveStored(v),v->GetName(),GetName());
			if (park)
				park->vehiclesStored--;
			#ifdef HAS_WINDOW
				UpdateIconLocations();
			#endif
		}

	#else


		bool						VehicleStore::StoreInPark(const PVehicle&v, float&atTime)
		{
			if (StoreLocally(v,atTime))
				return true;
			if (!park)
				return false;
			foreach (park->members,m)
				if (m->lock()->StoreLocally(v,atTime))
					return true;
			return false;
		}

	#endif

	/*virtual override*/ void				VehicleStore::OnDestroy(float atTime)
	{
		//Buffer0<WVehicleStore>	copy = sneighbors;
		Super::OnDestroy(atTime);	//disconnects from neighborhood. sneighbors should be empty now


		if (park)
		{
			for (const auto&w : park->members)
			{
				PVehicleStore s = w.lock();
				if (s && s.get() != this)
				{
					s->park.reset();
					#ifndef CLIENT
						while (localStore.IsNotEmpty() && s->StoreLocally(localStore.last().vehicle,atTime))
						{
							localStore.EraseLast();
						}
					#endif
				}
			}
			for (const auto&w : park->members)
			{
				PVehicleStore s = w.lock();
				if (s && s.get() != this && !s->park)
				{
					s->PropagatePark(PPark(new Park()));
				}
			}

		}
		#ifndef CLIENT
			for(const auto&v:localStore)
			{
				Remove(v.vehicle,RemovalType::DestroyedInStorage,atTime,false,true);
			}
		#endif
		localStore.Clear();

	}


	/*static*/ void Structure::InvalidateTerrain(const Grid::LCellIdI&location, float atTime)
	{
		bool changed[BaseGrid::NumFamilies] = {};
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;
		for (int j = 0; j < n.numNeighbors; j++)
		{
			changed[Grid::low.grid.GetCell(n.neighborID[j]).family] = true;
		}
		changed[Grid::low.grid.GetCell(location.stackID).family] = true;
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
			if (changed[i])
			{
				Grid::high.family[i].SetOutdated(atTime);
				Grid::low.family[i].SetOutdated(atTime);
			}
		#ifdef HAS_WINDOW
			Grid::actualWorld.SignalLightingChange(location);
		#endif

	}



	#ifndef CLIENT

	
		/*static*/	void Structure::CommonFixTerrain(const WorldEntity*e, const Grid::LCellIdI&location,  int broadHeight, int totalHeight, bool signalChange, float atTime)
		{
			#ifdef HYBRID_GRID
				const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(higher).neighborhood;
			#else
				const UINT32 higher = location.stackID;
			#endif
			//const Grid::Content::type_t foundationType = s.GetFoundationType();
			bool anyChanged = false;
			#ifdef HYBRID_GRID
				for (int j = 0; j < n.numNeighbors; j++)
				{
					Grid::CellStack&stack = Grid::actualWorld.cellStack[n.neighborID[j]];
					for (int a = 0; a < broadHeight; a++)
						if (stack.GetCellI(location.height+a).content != Grid::Content::Clear)
						{
							LogEvent(EventSignificance::Common,e,"Clearing cell "+Grid::HCellIdI(n.neighborID[j],location.height+a).ToString());

							anyChanged |= stack.MakeLocalHole(location.height+a);
						}
				}
			#endif
			Grid::CellStack&stack = Grid::actualWorld.cellStack[higher];
			//if (requireCentralBlock && stack.GetCellI(location.height - 1).GetStrength() < needStrength)
			//{
			//	LogEvent(EventSignificance::Common,e,"Strengthening cell "+Grid::HCellIdI(higher,location.height-1).ToString());
			//	anyChanged |= stack.LocalOverwrite(location.height - 1, foundationType);
			//	if (deep)
			//	{
			//		int h = location.height-2;
			//		int bottom = -Grid::CellStack::HalfRange;
			//		if (!Grid::CellStack::bottom.IsClearOrWaterIgnoreStructure())
			//			//bottom = h;
			//			while (h >= bottom && stack.GetCellI(h).IsClearOrWater())
			//			{
			//				anyChanged |= stack.LocalOverwrite(h,  Grid::CellStack::Cell::Content::Earth);
			//				h--;
			//			}
			//	}
			//}
			for (int a = 0; a < totalHeight; a++)
				if (stack.GetCellI(location.height+a).content != Grid::Content::Clear)
				{
					LogEvent(EventSignificance::Common,e,"Clearing cell "+Grid::HCellIdI(higher,location.height+a).ToString());

					anyChanged |= stack.MakeLocalHole(location.height+a);
				}

			//if (signalChange)	//i think this breaks the flow. calling objects/methods assume they have to do this manually
			//{
			//	if (broadFoundation)
			//	{
			//		for (int h = hChangedDownTo; h < location.height; h++)
			//			SignalClusterAndNeighborsChange(location + h,atTime);
			//	}
			//	else
			//		for (int h = hChangedDownTo; h < location.height; h++)
			//			SignalClusterChange(location + h,0,atTime,e);
			//}
			//

			if (anyChanged)
			{
				Grid::actualWorld.SignalChangeUpTo(location);	//execute this regardless of signalChange - world becomes invalid otherwise

				if (signalChange)
					Structure::InvalidateTerrain(location,atTime);
			}
		}


	#endif

	
	/*virtual*/ void				Structure::SignalLostNeighbor(const PStructure&other, float atTime)
	{
		if (connectedTo.FindAndErase(other))
		{
			LogEvent(EventSignificance::Common,nullptr, "Connection lost between "+GetName()+" and "+other->GetName());
			#ifdef HYBRID_GRID
				const UINT32 high = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(high).neighborhood;
				//foreach(connectedTo, c)

				const PStructure c = other;
				const UINT32 high2 = Grid::low.grid.GetCell((c)->location.stackID).higherEquivalent;
				const auto&n2 = Grid::high.grid.GetCell(high2).neighborhood;
				UINT32 between = BaseGrid::RequireIntersection(n, n2);
			#else
				UINT32 between = 0;
			#endif

			OnConnectionLost(other, between, atTime);
			#ifdef HAS_WINDOW
				changed = anyChanged = true;
			#endif
		}

	}


	/*virtual*/ bool				Structure::SignalEnvironmentChange(const TStructureNeighborhood&neighborhood, float atTime)
	{
		#ifndef CLIENT
			CorrectTerrain(false,atTime);
		#endif
		#ifdef HYBRID_GRID
			const UINT32 high = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			const auto&n = Grid::high.grid.GetCell(high).neighborhood;
		#endif
		
		auto _this = To<Structure>::Cast(shared_from_this());
		count_t limit = connectedTo.Count();
		foreach(neighborhood, candidate)
			if (!connectedTo.Contains(*candidate))
			{
				const PStructure c = *candidate;
				#ifdef HYBRID_GRID
					const UINT32 high2 = Grid::low.grid.GetCell((c)->location.stackID).higherEquivalent;
					const auto&n2 = Grid::high.grid.GetCell(high2).neighborhood;
					UINT32 between = BaseGrid::RequireIntersection(n, n2);
				#else
					UINT32 between = 0;
				#endif
				if (ConnectionPossibleTo(c, between) && c->ConnectionPossibleTo(_this,between))
				{
					connectedTo << c;
					#ifdef HAS_WINDOW
						changed = anyChanged = true;
					#endif
					OnNewConnection(c, between,atTime);
					c->connectedTo << _this;
					c->OnNewConnection(_this,between,atTime);
					LogEvent(EventSignificance::Common,nullptr, "Connection established between "+GetName()+" and "+c->GetName());

				}
			}

		for (index_t i = 0; i < limit; i++)
		{
			const PStructure c = connectedTo[i];
			#ifdef HYBRID_GRID
				const UINT32 high2 = Grid::low.grid.GetCell((c)->location.stackID).higherEquivalent;
				const auto&n2 = Grid::high.grid.GetCell(high2).neighborhood;
				UINT32 between = BaseGrid::RequireIntersection(n, n2);
			#else
				UINT32 between = 0;
			#endif
			if (!ConnectionPossibleTo(c, between) || !c->ConnectionPossibleTo(_this,between))
			{
				c->connectedTo.FindAndErase(_this);
				connectedTo.Erase(i--);
				limit--;
				#ifdef HAS_WINDOW
					c->changed = changed = anyChanged = true;
				#endif
				OnConnectionLost(c, between,atTime);
				c->OnConnectionLost(_this,between,atTime);
				LogEvent(EventSignificance::Common,nullptr, "Connection lost between "+GetName()+" and "+c->GetName());

			}
		}
		return true;
	}



	/*virtual override*/ bool		SkyPort::WouldLikeToEnable() const
	{
		return !!GetSatellite(location.stackID);
	}


	/*virtual override*/ void		SkyPort::GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&)	const
	{
		v.level[Resource::Energy] = -EnergyConsumption;
		if (MobileEntityProducer::queue.IsNotEmpty())
			v.level[Resource::Minerals] = -(int)MobileEntityProducer::productionPerRound;


	}


	#ifndef CLIENT
		/*virtual override*/ void			SkyPort::OnSleepChange(float&t)
		{
			Super::OnSleepChange(t);

			if (IsSleeping() && IsEnabled())
			{
				LogEvent(EventSignificance::Common,this,"Fell asleep. Disabling");
				SetEnabled(false,t);
			}
		}

		/*virtual override*/ void			VehiclePlant::OnSleepChange(float&t)
		{
			Super::OnSleepChange(t);

			if (IsSleeping() && IsEnabled())
			{
				LogEvent(EventSignificance::Common,this,"Fell asleep. Disabling");
				SetEnabled(false,t);
			}
		}

		/*virtual override*/ bool			VehiclePlant::DirectTo(CursorMode mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
			if (waypoints.Count() != 1)
				return false;
			MobileEntityProducer::UpdateGatheringPoint(waypoints.First());
			return true;
		}

		/*virtual override*/ bool			SkyPort::DirectTo(CursorMode mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
			if (waypoints.Count() != 1)
				return false;
			MobileEntityProducer::UpdateGatheringPoint(waypoints.First());
			return true;
		}
	#endif


	VehiclePlant::VehiclePlant(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner, location, atTime),
		MobileEntityProducer(DefaultProductionPerRound)
	{
		#ifdef CLIENT
			// make some base selected by default
			ASSERT__(baseComponents.IsNotEmpty())
			selectedBase = baseComponents[0];
		#endif
	}

	
	#ifndef CLIENT

		/*virtual override*/ void				VehiclePlant::Tick(ResourceVector&v)
		{
			v.level[Resource::Energy] -= EnergyConsumption;

			MobileEntityProducer::Tick(v);
		}





		/*virtual override*/ void			SkyPort::OnEnabledChange()
		{
			if (!this->IsEnabled())
			{
				if (connectedToHub)
				{
					//LogEvent(EventClass::Preset::operationAborted,this,"Connection to "+connectedTo->GetNiceName()+" lost");
					connectedToHub->LoseGroundConnection();
				}
			}
			else
				if (!connectedToHub)
				{
					PSkyHub hub = To<SkyHub>::DynamicCast(GetSatellite(location.stackID));
					if (hub && hub->owner == owner)
					{
						if (hub->GainGroundConnection(To<SkyPort>::Cast(shared_from_this())))
							ASSERT__(hub == connectedToHub);

					}
				}
		}


		/*virtual override*/ void				SkyPort::Tick(ResourceVector&v)
		{
			v.level[Resource::Energy] -= EnergyConsumption;
			if (MobileEntityProducer::queue.IsNotEmpty())
				MobileEntityProducer::Tick(v);



		}



		PVehicle					VehiclePlant::GetProduction() const
		{
			return To<Vehicle>::Cast(MobileEntityProducer::GetActiveProduction());
		}





	#else
		


		void				Tower::AppendTask(const Grid::LCellIdI&attackTarget)
		{
			AttackTask&task = attackQueue.Append();
			task.target = attackTarget;

			#ifdef HAS_WINDOW
				static Buffer0<TVec4<> > trajectory;
				trajectory.Clear();
				TVec3<> impactAt;
				Grid::HCellIdI dummy1;
				//if (Turret::TraceTrajectory(Turret::MaxRangeToVelocity(GetFlatAttackRange()),Grid::actualWorld, location,foundationHeight,attackTarget,trajectory,impactAt,dummy1))
				if (TraceTrajectory(Grid::actualWorld, attackTarget,trajectory,impactAt,dummy1))
				{

					static Geometry geo;
					geo.Clear();

					Component::Turret::CompileTrajectory(trajectory,Component::Turret::Schematic,geo,0.25f);

					if (task.trajectoryVisualization.shader.IsEmpty())
						(DX11::BaseInstructionBlock&)task.trajectoryVisualization = pathShader;
					task.trajectoryVisualization.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
					task.trajectoryVisualization.ibo.Create(geo.GetLOD(0).indices);


					((DX11::BaseInstructionBlock&) task.blastVisualization) = jobShader;
					task.blastVisualization.vbo = sphereVBO[0];
					task.blastVisualization.ibo = sphereIBO[0];
					//shape.depthTest = true;

					turret->BuildBlastSystem(trajectory.last().xyz * trajectory.last().w,task.blastSystem);
				}
			#endif
		}

	
		Component::VehicleBase*					VehiclePlant::GetProductionBase() const
		{
			return queue.IsNotEmpty() ? queue.first().base : nullptr;
		}



		#ifdef HAS_WINDOW


			/*virtual override*/ void			VehiclePlant::OnSelect()
			{
				gatheringPoint.UpdateVisuals();
			}
			/*virtual override*/ void			VehiclePlant::OnDeselect()
			{
				gatheringPoint.selected = Grid::LCellIdI::Invalid;
			}

			/*virtual override*/ void			SkyPort::OnSelect()
			{
				gatheringPoint.UpdateVisuals();
			}
			/*virtual override*/ void			SkyPort::OnDeselect()
			{
				gatheringPoint.selected = Grid::LCellIdI::Invalid;
			}

		#endif






		bool				VehiclePlant::EnqueueSelectedConstruction()
		{
			if (selectedBase && selectedTurret)
			{
				static Buffer0<Grid::LCellIdI>	choices;
				GetProductionChoices(this,selectedBase,choices);
				static bool ignoreWarnings = false;
				if (choices.IsNotEmpty() || ignoreWarnings)
					SignalConstructionBegin(id, selectedBase->constName, selectedTurret->constName);
				else
				{


					#ifdef HAS_WINDOW					
						String waterNotice;
						if (!selectedBase->moveOnWater)
							waterNotice = " Note that this vehicle type cannot stand on water. ";
						TUniqueID myID = id;
						ConstName baseName = selectedBase->constName;
						ConstName turretName = selectedTurret->constName;
						Dialog::Confirm("Cannot find valid spawning locations around '"+GetNiceName()+"' for '"+Vehicle::GetTypeName(selectedBase,selectedTurret)+"'. "+waterNotice+"Are you sure you wish to start this production?",
								[baseName,turretName,myID]()
								{
									SignalConstructionBegin(myID, baseName, turretName);
									//ignoreWarnings = true;
								}
								,
								[myID]()->Dialog::CheckResult
								{
									PVehiclePlant plant = To<VehiclePlant>::DynamicCast(GetEntity(myID));
									if (!plant || plant->health <= 0)
										return Dialog::CheckResult::Abort;
									if (!controlIsEnabled)
										return Dialog::CheckResult::OkayNotRightNow;
									return Dialog::CheckResult::OkayContinue;
								}
							);
					#else
						SignalConstructionBegin(id, selectedBase->constName, selectedTurret->constName);
					#endif
				}
			}
			return false;
		}
	
	#endif

	#ifndef CLIENT


		PVehicle				VehiclePlant::Enqueue(Component::VehicleBase*b, Component::Turret*t)
		{
			auto item = MobileEntityProducer::EnqueueProductionOfVehicle(b,t);
			return item ? To<Vehicle>::Cast(item->producing) : PVehicle();
		}
		PVehicle				VehiclePlant::Enqueue(ConstName baseName, ConstName turretName)
		{
			auto item = MobileEntityProducer::EnqueueProductionOfVehicle(baseName,turretName);
			return item ? To<Vehicle>::Cast(item->producing) : PVehicle();
		}


		bool				VehiclePlant::AbortConstruction(const TUniqueID&id)
		{
			return MobileEntityProducer::AbortConstruction(id);
		}


	#endif
	/*virtual override*/ void				VehiclePlant::PrototypeSetup()
	{
		Super::PrototypeSetup();
		baseComponents = Objects::Component::baseComponents;
		turretComponents = Objects::Component::vehicleTurretComponents;

		#ifdef HAS_WINDOW
				for (index_t i = 0; i < iconCommands.Count(); i++)
					if (iconCommands[i].name == "Enable")
					{
						iconCommands[i] = IconCommand(int2(4,1), "Enable",
							[](const PWorldEntity&e)
							{

								VehiclePlant*s2 = dynamic_cast<VehiclePlant*>(e.get());
								if (s2)
								{
									if (!s2->IsEnabled() && !s2->CanEnable())
									{
										SignalUserMessage(s2->GetNiceName()+" is switched off because its queue is empty. It will attempt to automatically enable once you enqueue at least one production job using the 'Construct Selected' button. You must first select both a foundation and a turret, however.");
										return;
									}
									SignalEnableStructure(*s2, !s2->IsEnabled());
								}
							}
							,[](const PWorldEntity&e, bool controlEnabled)
							{
								BaseCommand::Flags rs = BaseCommand::Flags::None;
								VehiclePlant*div = dynamic_cast<VehiclePlant*>(e.get());
								if (controlEnabled && div != nullptr)
									rs |= BaseCommand::Flags::IsEnabled;
								if (Cmd::IsEnabled(e))
									rs |= BaseCommand::Flags::IsPermaActive;
								return rs;
							}
							,NoCosts
						);
						break;
					}



			iconCommands << IconCommand(int2(6,0), "Construct Selected",[](const PWorldEntity&e)
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(e.get());
					if (plant)
					{
						if (plant->selectedBase && plant->selectedTurret)
							plant->EnqueueSelectedConstruction();
						else
						{
							if (plant->selectedBase)
								SignalUserMessage(plant->GetNiceName()+" needs to know what turret you want to use in order to enqueue a new production job. Please select one in the top row.");
							elif (plant->selectedTurret)
								SignalUserMessage(plant->GetNiceName()+" needs to know what foundation you want to use in order to enqueue a new production job. Please select one in the bottom row.");
							else
								SignalUserMessage(plant->GetNiceName()+" needs to know what turret and foundation you want to use in order to enqueue a new production job. Please select one in each top and bottom rows.");
						}
					}
				},
				[](const PWorldEntity&e, bool controlEnabled)
				{
					if (!controlEnabled)
						return BaseCommand::Flags::None;
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(e.get());
					return (plant != nullptr/* && plant->selectedBase && plant->selectedTurret*/)
						? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None
						;
				},
				[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
				{
					PVehiclePlant p = To<VehiclePlant>::DynamicCast(e);
					ASSERT__(p);
					if (!p->selectedBase || !p->selectedTurret)
					{
						mineralCostOut = 0;
						timeCostOut = 0;
						return;
					}
					mineralCostOut = Vehicle::GetPrice(p->selectedBase,p->selectedTurret);
					timeCostOut = p->ProductionRounds(mineralCostOut,false);
				}
			);

			iconCommands << IconCommand(int2(5,1), "Sleep",VehicleCmd::Sleep,VehicleCmd::SleepCheck,NoCosts);

			int at = 0;
			//vehicleConstructionCommands << VehicleConstructionCommand("Base:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);
			foreach (baseComponents,comp)
			{
				Component::VehicleBase* c = *comp;
				vehicleConstructionCommands << VehicleConstructionCommand(int2(7+at,0), c->typeName,c,nullptr,[c](const PWorldEntity&e)
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(e.get());
					if (plant)
						plant->selectedBase = c;
				}
				,[c](const PWorldEntity&e, bool controlEnabled)
				{
					BaseCommand::Flags rs = CommonCmd::CommonButtonCheck(e,controlEnabled);
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(e.get());
					if (plant && plant->selectedBase == c)
						rs |= BaseCommand::Flags::IsPermaActive;
					return rs;
				});
				at++;
			}
			at = 0;
			//commands << Command("Turret:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);
			foreach (turretComponents,comp)
			{
				Component::Turret* c = *comp;
				vehicleConstructionCommands << VehicleConstructionCommand(int2(6+at,1), c->typeName,nullptr,c,[c](const PWorldEntity&e)
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(e.get());
					if (plant)
						plant->selectedTurret = c;
				}
				,[c](const PWorldEntity&e, bool controlEnabled)
				{
					BaseCommand::Flags rs = CommonCmd::CommonButtonCheck(e,controlEnabled);
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(e.get());
					if (plant && plant->selectedTurret == c)
						rs |= BaseCommand::Flags::IsPermaActive;
					return rs;
				});
				at++;
			}
		#endif

		//commands << Command("Queue:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);

	}
	#ifdef HAS_WINDOW
		/*virtual override*/ void				VehiclePlant::VisitAllCommands(FCommandVisitor visitor)
		{
			MobileEntityProducer::VisitAllCommands(visitor);
			Super::VisitAllCommands(visitor);
		}
		/*virtual override*/ void				SkyPort::VisitAllCommands(FCommandVisitor visitor)
		{
			MobileEntityProducer::VisitAllCommands(visitor);
			visitor(skyHubConstructionCommand);
			visitor(formerConstructionCommand);
			Super::VisitAllCommands(visitor);
		}

	#endif
	/*virtual override*/ bool				VehiclePlant::AwaitsUserInteraction() const 
	{
		return  MobileEntityProducer::AwaitsUserInteraction();
	}


	/*virtual override*/ bool				SkyPort::AwaitsUserInteraction() const 
	{
		return MobileEntityProducer::AwaitsUserInteraction();
	}



	/*virtual override*/ void				SkyPort::PrototypeSetup()
	{
		Super::PrototypeSetup();

		#ifdef HAS_WINDOW
	
			//vehicleConstructionCommands << VehicleConstructionCommand("Base:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);
			//foreach (baseComponents,comp)
			{
				//VehicleBase* c = *comp;
				formerConstructionCommand = VehicleConstructionCommand(int2(5,0), "Construct Hover-Former", &Component::hoverbase,&Component::terraformer, [](const PWorldEntity&e)
				{
					PSkyPort port = To<SkyPort>::DynamicCast(e);
					if (!port)
						return;
					SignalConstructionBegin(port->id, ConstName::VehicleBase_HoverBase, ConstName::Turret_Terraformer);
				}
				,CommonCmd::CommonButtonCheck);
				
				skyHubConstructionCommand = SatelliteConstructionCommand(int2(5,1), "Construct SkyHub", SkyHub::globalPrototype, [](const PWorldEntity&e)
				{
					PSkyPort port = To<SkyPort>::DynamicCast(e);
					if (!port)
						return;
					SignalConstructionBegin(port->id, ConstName::Type_SkyHub, ConstName::Empty);
				}
				,
				[](const PWorldEntity&e, bool controlsEnabled)
				{
					if (!controlsEnabled)
						return BaseCommand::Flags::None;
					return !GetSatellite(e->location.stackID) ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
				});
			}
		#endif

		//commands << Command("Queue:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);

	}



	
	/*virtual override*/ bool				NexusPort::ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)
	{
		#ifndef CLIENT
			return (connectedTo.IsEmpty() || other->owner == connectedTo.first()->owner);
		#else
			return true;
		#endif
	}

	/*virtual override*/ void				NexusPort::OnNewConnection(const PStructure&other, UINT32 acrossHighCell, float atTime)
	{
		#ifndef CLIENT
			if (owner != other->owner && !RuntimeConfig::loadingPhase)
			{
				resistence = ResistenceRounds;
				SetEnabled(false,atTime,false);
				ChangeOwnership(shared_from_this(), other->owner,atTime);
			}
		#endif
	}

	/*virtual override*/ void				NexusPort::OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime)
	{
		#ifndef CLIENT
			if (connectedTo.IsEmpty() && owner != Faction::nexus && !RuntimeConfig::loadingPhase)
			{
				SetEnabled(false,atTime,false);
				OwnerParameter old = owner;
				ChangeOwnership(shared_from_this(), Faction::nexus,atTime);
			}
		#endif
	}
	#ifndef CLIENT
		void					NexusPort::OnLoad()
		{
			if (resistence > 0)
				owner->acquiringNexusControl++;
			else
				owner->nexusControl++;
		}

		/*virtual override*/ bool			NexusPort::ConnectsToDifferentOwnership() const
		{ 
			return true;
		}


		/*virtual override*/ void			NexusPort::Tick(ResourceVector&v)
		{
			if (owner != Faction::nexus && resistence > 0)
			{
				RegisterChange(NexusControlChanged,0);
				resistence --;
				if (!resistence)
				{
					owner->acquiringNexusControl --;
					DBG_ASSERT__(owner->acquiringNexusControl >= 0);
					owner->nexusControl ++;
					LogEvent( EventClass( EventSignificance::Important,EventFlavor::Gain ),this,"Control Acquired");
					SignalNexusControlChange(owner);
				}
			}

			ResourceVector v1, s, m;
			GetResourceVector(v1,s,m);
			v += v1;

		}

		/*virtual override*/ void			NexusPort::OnEnabledChange()
		{
			if (!RuntimeConfig::loadingPhase)
			{
				if (owner != Faction::nexus)
				{
					if (!IsEnabled())
					{
						if (resistence > 0)
						{
							DBG_ASSERT__(owner->acquiringNexusControl > 0);
							owner->acquiringNexusControl--;
						}
						else
						{
							DBG_ASSERT__(owner->nexusControl > 0);
							owner->nexusControl--;
						}
						LogEvent(EventClass( RuntimeConfig::loadingPhase ? EventSignificance::Common : EventSignificance::Important,EventFlavor::Loss ),this,"Lost control");
					}
					else
					{
						owner->acquiringNexusControl++;

						LogEvent(EventClass( RuntimeConfig::loadingPhase ? EventSignificance::Common : EventSignificance::Uncommon,EventFlavor::Gain ),this,"Acquiring control");

					}
					resistence = ResistenceRounds;
					RegisterChange(NexusControlChanged,0);
					SignalNexusControlChange(owner);
				}
			}
		}
	#endif


	

	#ifndef CLIENT
		bool					compoundCoverageChanged = false;

		bool		RefreshCompoundCoverageIfNeeded( float atTime)
		{
			if (compoundCoverageChanged)
			{
				compoundCoverageChanged = false;
				RefreshCompoundCoverage(atTime);
				return true;
			}
			return false;
		}
	
	#endif

	void RefreshCompoundCoverage( float atTime) 
	{
		//LogEvent(EventSignificance::Debug,nullptr,"Refreshing network coverage");

		#ifndef ONE_COMPOUND_PER_FACTION

			compounds.Clear();
			Queue<Structure*>	primary,secondary;
			foreach (structures,s)
			{
				(*s)->compoundID = Compound::InvalidNetwork;
				secondary << s->get();
			}
		
			Structure*s;
			while (secondary >> s)
			{
				if (s->compoundID != Compound::InvalidNetwork)
					continue;
				//PNetwork network(new Compound());
				const UINT32 compoundID = (UINT32)compounds.Count();
				Compound&network = compounds.Append();
				primary << s;
				while (primary >> s)
				{
					if (s->compoundID != Compound::InvalidNetwork)
						continue;
					s->compoundID = compoundID;
					network.AppendStructure(s);
					foreach (s->connectedTo,c)
						if ((*c)->compoundID == Compound::InvalidNetwork)
							primary << c->get();
				}
				network.Compact();
			}

		#else
			#ifdef CLIENT

				for (const auto&faction : Faction::allKnown)
				{
					faction->compound.containedStructures.Clear();
				}

				for (const auto&s:structures)
				{
					s->owner->compound.AppendStructure(s.get());
				}
			#else
//				compounds.SetSize(Faction::all...)
				for (const auto&f0 : Faction::all)
				{
					PFaction faction = f0.lock();
					DBG_ASSERT__(faction);
					if (faction)
					{
						faction->UpdateCompoundCoverage();
					}
				}

				Faction::nexus->UpdateCompoundCoverage();

			#endif


		#endif

		RefreshCompoundStates(atTime);
		#ifdef HAS_WINDOW
			OnSelectionChange(true);
		#endif
	}



	#ifdef HAS_WINDOW
	

		/*virtual override*/ void			NexusPort::DescribeType(BaseCommand::Label&label, UINT32 descFlags)
		{
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label.WrapAppendText("Nexus Ports are indestructible structures of unknown origin."
			" Depending on game-settings, non-player Nexus Ports serve as spawning-beakons for Nexus units."
			" Nexus Ports will shift ownership to the respective owner of the first connected player building."
			" They must be enabled for a number of rounds in order to count towards winning the game."
			" Disabling a Nexus Port will make it stop counting as such."
			" A Nexus Port that loses all connections to other player-owned structures will shift back to"
			" the Nexus faction."
			);
			
		
		}


		/*virtual override*/ void			Mine::DescribeType(BaseCommand::Label&label, UINT32 descFlags)
		{
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label.WrapAppendText("Enabled mines extract any natural resources available in the planet-core beneath their location. They are essential in supplying other structures in the network."
			" Mines should be build only on known occurrences of natural resources."
			" They cannot be constructed atop each other, since they require a working link to the core in order to operate."
			" Mines, like many structures, require "+String(ResourceTag(Resource::Energy))+" from generators to operate."
			);
		}



		/*virtual override*/ void			Generator::DescribeType(BaseCommand::Label&label, UINT32 descFlags)
		{
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label.WrapAppendText("Enabled generators provide energy for the local compound."
			" Since many structures consume "+String(ResourceTag(Resource::Energy))+" when enabled, generators are a crucial component of any active base."
			" Combine with reactor cores for increased output."
			);
		}

		/*virtual override*/ void			ReactorCore::DescribeType(BaseCommand::Label&label, UINT32 descFlags)
		{
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label.WrapAppendText("Amplifies the energy output of connected generators. Only one reactor core can be connected to any one generator, but multiple generators may be connected to the same reactor core."
			);
		}
		/*virtual override*/ void			VehiclePlant::DescribeType(BaseCommand::Label&label, UINT32 descFlags)
		{
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label.WrapAppendText("Enabled vehicle plants produce vehicles of arbitrary configuration at the expense of "
			+String(ResourceTag(Resource::Minerals))+", "+String(ResourceTag(Resource::Energy))+", and "+String(TimeResourceTag())+"."
			" A vehicle plant that does not have access to the necessary resources switches off immediately."
			" Vehicle plants should be build such, that the intended vehicle output can stand in the immediate surroundings of the structure."
			);
		}


		/*virtual override*/ void			WorkingStructure::DescribeType(BaseCommand::Label&label, UINT32 descFlags)
		{
			Super::DescribeType(label,  descFlags);

			ResourceVector balance,discard,maxStorage;

			GetResourceVector(balance, discard, maxStorage);

			if (balance.IsNotEmpty())
			{
				if (balance.SumNegative())
				{
					label << "Consumes:";
					for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
						if (balance.level[i] < 0)
						{
							label << " "+String(ResourceTag(i))+String(-balance.level[i]);
						}
				}
				if (balance.SumPositive())
				{
					label << "Produces:";
					for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
						if (balance.level[i] > 0)
						{
							label << " "+String(ResourceTag(i))+String(balance.level[i]);
						}
				}
			}
			if (maxStorage.IsNotEmpty())
			{
				label << "Storage capacity:";
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					if (maxStorage.level[i] > 0)
					{
						label << " "+String(ResourceTag(i))+String(maxStorage.level[i]);
					}
			}
		
		}



	#endif


	
	bool						WorkingStructure::CouldEnable(bool ignoreCanEnable/*=false*/)
	{
		if (health <= 0)
			return false;
		if (IsEnabled())
			return true;

		const Compound&n = GetCompound();
		String dummy;
		return n.CanEnable(this,dummy,ignoreCanEnable);
	}


	/*virtual override*/ void				WorkingStructure::OnDestroy(float atTime)
	{
		Super::OnDestroy(atTime);
	}

	#ifndef CLIENT

		/*virtual override*/ void			WorkingStructure::OnSleepChange(float&t)
		{
			if (!IsSleeping() && !isEnabled && CanEnable())
			{
				LogEvent(EventSignificance::Common,this,"Woke up disabled. Trying to enable");
				SetEnabled(true,t);
			}
		}


		bool						WorkingStructure::SetEnabled(bool b, float atTime, bool refreshNetwork/*=true*/, force_t force/*=EnableIfPossible*/, bool signalStructure/*=true*/)
		{
			if (isEnabled == b)
			{
				LogEvent(EventSignificance::Common,this,"Requested structure enable-state already set: "+String(b)+". Notifying client. Ignoring command.");
				RegisterChange(EnableChanged,atTime);	//some state error, must be corrected
				return true;
			}
			if (b && (force==EnableIfPossible && !CanEnable()))
			{
				LogEvent(EventClass::Preset::operationAborted,this,"Cannot enable at this time");
				RegisterChange(CanEnableChanged,atTime);	//might be off...
				return false;
			}
			if (b && refreshNetwork)
			{
				Compound::lastDisabled = nullptr;
				RefreshCompoundCoverageIfNeeded(atTime);
//				if (compoundID < compounds.Count())
				{
					Compound&network = GetCompound();
					String outWhy;
					if (!network.CanEnable(this,outWhy))
					{
						LogEvent(EventClass::Preset::operationAborted,this,"Cannot enable: Insufficient resource supply in compound ("+outWhy+")");
						return false;
					}
				}
				//else
				//{
				//	LogEvent(EventClass::Preset::error,this,"Compound ID out of bounds: "+String(compoundID)+"/"+String(compounds.Count()));
				//	return false;
				//}
			}
			if (!b)
				Compound::lastDisabled = this;
			else
			{
				Compound::lastDisabled = nullptr;
				SetSleep(false,atTime,false);
			}
			isEnabled = b;
			RegisterChange(EnableChanged,atTime);
			if (signalStructure)
				OnEnabledChange();
			if (refreshNetwork)
			{
				RefreshCompoundCoverageIfNeeded(atTime);
				GetCompound().RefreshState(atTime);
				//if (compoundID < compounds.Count())
				//	compounds[compoundID].RefreshState(atTime);
				//else
				//{
				//	LogEvent(EventClass::Preset::error,this,"Compound ID out of bounds: "+String(compoundID)+"/"+String(compounds.Count()));
				//}
			}
			return true;
		}
	#else
		void					WorkingStructure::ApplyEnableChangeFromServer(bool b, bool temporary)
		{
			isEnabled = b;
			#ifdef HAS_WINDOW
				if (!temporary)
					ambientSound.SetGain(GetSoundLevel());
			#endif

		}

	#endif


}
