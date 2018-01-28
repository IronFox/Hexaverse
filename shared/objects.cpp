#include <global_root.h>
#include "objects.h"

#ifdef HAS_WINDOW
	#include <engine/display.h>
#endif

//#include <geometry/graph_mesh.h>
#include <geometry/cgs.h>
#include <geometry/converter/obj.h>
#include "stringTable.h"
#include "logEvent.h"
#include "runtimeConfig.h"
#include "objects/structure/mine.h"
#include "objects/structure/laboratory.h"
#include "objects/structure/storageStructures.h"
#include "objects/structure/tower.h"
#include "objects/structure/vehiclePlant.h"
#include "objects/structure/vehicleStore.h"
#include "objects/pathfinding.h"
#include "globalEvents.h"


namespace Objects
{
	bool preserveReferencesOnFlush = false;

	#ifndef NO_SATELLITES
		void RefreshSkyHubConnections()
		{
			for (const auto&sat : satellites)
			{
				if (sat->IsSkyHub())
					To<SkyHub>::Cast(sat)->UpdateConnections();
				#ifdef HAS_WINDOW
					sat->FlushFuelDemandCache();
				#endif
			}
			#ifdef HAS_WINDOW
				OnSelectionChange(true);
			#endif
		}
	#endif


	#ifdef CLIENT

		#ifdef THOROUGLY_DEBUG_SENSORY
			void	VerifySensory()
			{
				foreach (Grid::cellStack,stack)
					stack->ClearDbgSensory();
				foreach (structures,s)
					if ((*s)->isOwnedByMe)
						Grid::InsertDbgSensor((*s)->location,(*s)->GetScanningRange());
				foreach (vehicles,s)
					if ((*s)->isOwnedByMe)
						Grid::InsertDbgSensor((*s)->location,(*s)->GetScanningRange());
				foreach (Grid::cellStack,stack)
					stack->CheckDbgSensory();

			}
		#endif




	#else

	











	#endif


	void					Destroy();

	void					Create()
	{
		#ifdef CLIENT
			#ifndef NO_SATELLITES
				Grid::visibleOrbitGrid.orbit = Grid::ToRadius(Satellite::BaseResidenceHeight);
				Grid::visibleOrbitGrid.Clear();
			#endif
		#else

			World::actual.onTerrainChange << [](const World::Data&d, const BaseGrid::FamiliesChanged&changed, float t){
				Objects::ForeachWorldEntity([changed,t](const Objects::PWorldEntity&e)
				{
					if (e->IsAboveGrid())
						return;
					if (changed.IsSet( Grid::low.grid.GetCell(e->location.stackID).family ))
					{
						if (!e->ClearOccupiedTerrain(t))
						{
							float t2 = t;
							Remove(e,Objects::RemovalType::UncontrolledExplosion,t2);
						}
						else
						{
							if (e->IsMobile())
								To<MobileEntity>::Cast(e)->CheckGround(t);	//check if mobile entity should fall
						}
					}
				},false);
			};

			//#ifdef _DEBUG
			//	Faction::nexus->accessToken = CryptographicID();
			//	LogEvent(EventClass::Preset::uncommon, nullptr, "Debug: Set nexus access token to " + Faction::nexus->accessToken.ToString() + "...");
			//#endif

			VehicleJob::BaseJob::allJobPrototypes
				<< PVehicleJob(new VehicleJob::BuildRoad())
				<< PVehicleJob(new VehicleJob::AlterTerrain())
				<< PVehicleJob(new VehicleJob::BuildWallUp())
				<< PVehicleJob(new VehicleJob::BuildHorizontalWall())
				//<< PVehicleJob(new VehicleJob::BuildFoundation())
				#ifndef ONE_COMPOUND_PER_FACTION
					<< PVehicleJob(new VehicleJob::TransferJob())
				#endif
				<< PVehicleJob(new VehicleJob::RepairJob())
				<< PVehicleJob(new VehicleJob::ClearAbove())
				<< PVehicleJob(new VehicleJob::DigHorizontal())
				<< PVehicleJob(new VehicleJob::DigDown())
				<< PVehicleJob(new VehicleJob::Construct())
				<< PVehicleJob(new VehicleJob::Sequence())
				<< PVehicleJob(new VehicleJob::SelfDetonate())
				;

		#endif
		LogEvent(EventSignificance::PerformanceMeasurement, nullptr, "Vehicle grid of "+String(vehicleGrid.GetContentSize())+" bytes allocated");
		LogEvent(EventSignificance::PerformanceMeasurement, nullptr, "Structure grid of "+String(structureGrid.GetContentSize())+" bytes allocated");

		LogEvent(EventSignificance::Debug, nullptr, "Structure static setup");
		Structure::StaticSetup();

		LogEvent(EventSignificance::Debug, nullptr, "Registering prototypes");
		Prototypes::RegisterAll();



		Hangar::StaticSetup();

		foreach (Prototypes::all,p)
			(*p)->PrototypeSetup();

		ResearchState::StaticCreate();	//all prototypes good. time to set the tables

		Pathfinding::Init();



		GlobalEvents::objectFlush << []()
		{
			LogEvent(EventClass::Preset::uncommon,"Flushing scene data");

			if (!preserveReferencesOnFlush)
			{
				//These flushes should reduce the impact of stray entities. As it turns out, there are some entities that remain alive due to some reference somewhere.
				//Either there is a global reference somewhere (most likely), or an actual memory leak. Happens during first loading, Does not seem to get worse during subsequent loadings.
				//Global entities that are not erased remain the same, at the exact same address. They are not replaced, or cleared.
				//Therefor:
				//Remove references of factions to alliances (which should erase all alliances)
				ForeachFaction([](const PFaction&f)
				{
					f->alliance->Leave(f);
				});
				//Remove references of entities to factions (which, coupled with the deletion of all global faction references should erase all factions)
				ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
				{
					e->owner.reset();
				},true);
			}

			storedMobileEntities.Clear();
			#ifndef NO_SATELLITES
				satellites.Clear();
			#endif
			vehicles.Clear();
			structures.Clear();
			idMap.Clear();
			#ifndef ONE_COMPOUND_PER_FACTION
				compounds.Clear();
			#endif
			vehicleGrid.Fill(PVehicle());
			structureGrid.Fill(PStructure());
			#ifdef CLIENT
				skyLinks.Clear();
				Marker::FlushAll();
				#ifndef NO_HIBERNATION
					uplinkTraffic = 0;
					onlineUplinkTraffic = 0;
				#endif



			#else
				Faction::all.Clear();
				Faction::nexus.reset();
				Alliance::FlushAll();	//Remove remaining alliances, if any (due to the clearing above, no remaining ones have been observed but just to be perfectly sure, they are dumped anyways
				Faction::RecreateNexus();
			#endif
		};

		GlobalEvents::applicationTerminate << Destroy;

		GlobalEvents::terrainFlush << []()
		{
			#ifdef CLIENT	//temporary. once i figured out good values the limit will be restored from file and clearing is valid for server too
				RuntimeConfig::UnitLimits::Reset();
			#endif
			RuntimeConfig::hibernationSystemEnabled = false;
		};


		//globalCommands.Append(Command("Clip Terrain", &Cmd::ToggleSeeThrough, &AlwaysTrue, &Cmd::SeeThroughIsActive));
	}






	void					Destroy()
	{
		#ifdef CLIENT
			skyLinks.Clear();
		#endif
		vehicles.Clear();
		structures.Clear();
		Prototypes::Destroy();
		Component::DestroyAll();
	}
	
	#ifdef VOXEL_SCANNER
		class RayWalker
		{
		public:	
			M::TVec3<> atPos;
			const M::TVec3<> toPos;
			Grid::LCellIdI atCell;
			const Grid::LCellIdI toCell;
			count_t cellsTouched;
			bool	shaded;

			/**/	RayWalker(const Grid::LCellIdI&from, const Grid::LCellIdI&to, const M::TVec3<>&fromLocation, const M::TVec3<>&toLocation):
				atPos(fromLocation),toPos(toLocation),atCell(from),toCell(to),cellsTouched(0),shaded(false)
			{

			}
	
	
			bool Walk()
			{
				if (atCell == toCell)
					return false;
				cellsTouched++;
				auto&cell = Grid::lowCellStack[atCell.stackID.volumeCell[Grid::CellStack::ToLayer(atCell.height)];
				if (cell.visibility == cell.Visible)
					return false;
				if (cell.visibility == cell.Invisible)
				{
					shaded = true;
					return false;
				}
				if (cell.isFull)
				{
					shaded = true;
					return false;
				}
				float d2 = Vec::quadraticDistance(toPos,atPos);
				Grid::LCellIdI candidate = atCell;

				int lower = std::max(atCell.height -1, -Grid::CellStack::HalfRange);
				int upper = std::min(atCell.height+1,Grid::CellStack::HalfRange-1);
				for (int h = lower; h <= upper; h++)
				{
					UINT layer = Grid::CellStack::ToLayer(h);
					const auto&n = Grid::low.grid.GetCell(atCell.cellID).neighborhood;
					for (BYTE k = 0; k < n.numNeighbors; k++)
					{
						Grid::LCellIdI id(n.neighborID[k],h);
						const M::TVec3<> remote = Grid::low.cells[id.stackID.centerVertex.xyz * Grid::ToRadius(id.height);
						float cd2 = Vec::quadraticDistance(remote,toPos);
						if (cd2 < d2)
						{
							d2 = cd2;
							candidate = id;
						}
					}
					if (h != atCell.height)
					{
						Grid::LCellIdI id(atCell.cellID,h);
						const M::TVec3<> remote = Grid::low.cells[id.stackID.centerVertex.xyz * Grid::ToRadius(id.height);
						float cd2 = Vec::quadraticDistance(remote,toPos);
						if (cd2 < d2)
						{
							d2 = cd2;
							candidate = id;
						}
					}
				}
								
				atCell = candidate;
				atPos = Grid::low.cells[atCell.stackID.centerVertex.xyz * Grid::ToRadius(atCell.height);
				return true;
			}
	
		};
	#endif

	#ifdef CLIENT

		bool	SphereEdgeDetection(const M::TVec3<>&center, float radius, const M::TVec3<>&point0, const M::TVec3<>&point1 )
		{
			return M::Obj::DetectSphereEdgeIntersection(center,radius,point0,point1);
		
		}
	

		#ifdef HAS_WINDOW


		#endif
	
	
	#endif


	//bool FindShortestPath(const Vehicle*v, Grid::LCellIdI destination, BasicBuffer<Grid::LCellIdI>&outPath);
	






	//void					Vehicle::AbortJob()
	//{
	//	{
	//		LogEvent("Aborted "+jobDescription);
	//		job = FJob();
	//		roundsToJobCompletion = -1;
	//		jobDescription.free();
	//		unstable = false;
	//	}
	//}









#if 0


	/**/						Road::Road(const Grid::LCellIdI&at) :Structure(at, RoadClass::staticInstance)
	{}

	/*virtual override*/ PStructureClass	Road::GetClass() const
	{
		return RoadClass::staticInstance;
	}


	/*virtual override*/ void				Road::OnNewConnection(const PStructure&to, UINT32 acrossHighCell)
	{}

	/*virtual override*/ void				Road::OnConnectionLost(const PStructure&to, UINT32 acrossHighCell)
	{}



	/*virtual override*/ bool	RoadClass::CanPlace(const Grid::LCellIdI&location)
	{
		const auto&stack = Grid::cellStack[Grid::low.grid.GetCell(location.cellID).higherEquivalent];
		return NoStructureHere(location) && stack.GetCellI(location.height - 1).IsSolid() && Grid::LowCellIsClear(location);
			//!stack.GetCellI(location.height).IsOccupied();
	}
	/*virtual override*/ PStructure			RoadClass::CreateInstance(const Grid::LCellIdI&location)
	{
		PStructure rs(new Road(location));
		return rs;
	}
	/*virtual override*/ void	RoadClass::Setup()
	{
		MakeBox3(connectorMesh, M::Box<>(-0.7f, 0.f, 0.f, 0.7f, 1.f, 0.11f), float4(float3(0.25f), 1.f));
		MakeCylinder3(centralMesh, float3(), 0.7f, 0.1f, float4(float3(0.25f), 1.f));
	}

	/*virtual override*/ bool Road::ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)
	{
		if (other->GetClass() != GetClass())
			return false;
		if (other->location.height != this->location.height)
			return false;
		return !Grid::cellStack[acrossHighCell].GetCellI(location.height).IsOccupied();
	}



	/*virtual override*/ void				Road::Recompile()
	{
		changed = false;

		LogEvent("Recompiling " + GetName());
		localMesh.ClearCompletely();




		/*

		for later:
		SurfaceDescription	desc;

		for (index_t i = 0; i + 1 < connectedTo.Count(); i++)
		for (index_t j = i + 1; j < connectedTo.Count(); j++)
		{
		const PStructure&n0 = connectedTo[i];
		const PStructure&n1 = connectedTo[j];
		SurfaceDescription::TConnector	c0;
		const float r0 = (Grid::ToRadius(location.height) + Grid::ToRadius(n0->location.height)) / 2.f;
		const float r1 = (Grid::ToRadius(location.height) + Grid::ToRadius(n1->location.height)) / 2.f;
		c0.state.position = Grid::low.grid.GetCell(n0->location.cellID).center * r0;
		c1.state.position = Grid::low.grid.GetCell(n1->location.cellID).center * r1;


		M::TVec3<>			position,
		direction,
		up;				//!< Up axis. Must be normalized and orthogonal to @a direction
		M::TVec2<>			scale;
		float			angle0,
		angle1,
		texcoord0,
		texcoord1;


		}

		foreach(this->connectedTo, c)
		{





		}


		*/












		
		const float l0 = Grid::GetAverageLightLevel(location);
			//Grid::GetHighestLightLevel(location);
			//Grid::cellStack[Grid::low.grid.GetCell(location.cellID).higherEquivalent].GetCellI(location.height).GetLightLevel();
		{
			const Geometry&source = RoadClass::staticInstance->connectorMesh;


			for (index_t lod = 0; lod < source.CountLODs(); lod++)
			{
				const Geometry::LOD&data = source.GetLOD(lod);
				//const Vertex*vertexData = data.vertices.pointer();
				localMesh.AppendAndActivateNewLOD();
				const count_t numVertices = data.vertices.Count();
				foreach(this->connectedTo, c)
				{
					if ((*c)->location.stackID> location.cellID)
						continue;	//only one direction
					const float l1 = Grid::GetAverageLightLevel((*c)->location);
						//Grid::cellStack[Grid::low.grid.GetCell((*c)->location.cellID).higherEquivalent].GetCellI((*c)->location.height).GetLightLevel();
					float light = std::max(l0, Grid::GetHighestLightLevel((*c)->location));
					localMesh.SetVertexOffsetToCurrent();
					const float3&b0 = Grid::low.grid.GetCell(this->location.cellID).center;
					const float3&b1 = Grid::low.grid.GetCell((*c)->location.cellID).center;
					const float3 d = b1 - b0;
					const float scale = d.GetLength();
					const float3 d1 = d / scale;
					const float r0 = Grid::ToRadius(location.height);
					const float r1 = Grid::ToRadius(location.height + 1);

					const float3 x1 = -(b0 | d1).Normalized();
					const float3 x = x1 * (r1 - r0);
					for (index_t i = 0; i < numVertices; i++)
					{
						const Vertex&vtx = data.vertices[i];
						const M::TVec3<>&position = vtx.vertex;
						float r = r0 + (r1 - r0) * position.z;
						M::TVec3<> np = b0 + d * position.y + x * position.x / r;
						Vec::normalize(np);
						float3 up = np;
						np *= r;
						float4 color = vtx.color;
						color.rgb *= //light;
							(l0 + (l1 - l0)*position.y);
						localMesh.MakeVertex(np, up * vtx.normal.z + x * vtx.normal.x + d1 * vtx.normal.y, color);	//dont care about uv for now
					}
					localMesh.AppendTriangleIndices(data.indices);
				}
				//localMesh.ComputeNormals();
			}
		}
		//if (connectedTo.Count() != 1)
		{
			const Geometry&source = RoadClass::staticInstance->centralMesh;

			for (index_t lod = 0; lod < source.CountLODs(); lod++)
			{
				const Geometry::LOD&data = source.GetLOD(lod);
				//const Vertex*vertexData = data.vertices.pointer();
				localMesh.SetLOD(lod);
				const count_t numVertices = data.vertices.Count();

				localMesh.SetVertexOffsetToCurrent();
				M::TMatrix4<> system = M::Matrix<>::eye4;
				const float r0 = Grid::ToRadius(this->location.height);
				Mat::makeAxisSystem(Grid::low.grid.GetCell(this->location.cellID).center*r0, Grid::low.grid.GetCell(this->location.cellID).center, 2, system);
				const float scale = Grid::ToRadius(this->location.height + 1) - r0;
				system.x.xyz *= scale;
				system.y.xyz *= scale;
				system.z.xyz *= scale;
				for (index_t i = 0; i < numVertices; i++)
				{
					const Vertex&vtx = data.vertices[i];
					const M::TVec3<>&position = vtx.vertex;
					M::TVec3<> np, n;
					np = system.w.xyz + system.x.xyz * position.x + system.y.xyz * position.y;
					Vec::normalize(np);
					system.z.xyz = np * scale;
					np *= r0 + scale * position.z;
					Mat::rotate(system, vtx.normal, n);
					n /= scale;

					//	Vec::normalize(np);
					//np *= Grid::ToRadiusf(float(location.height) + position.z);

					float4 color = vtx.color;
					color.rgb *= l0;

					localMesh.MakeVertex(np, n, color);	//dont care about uv for now
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



}

