#include <global_root.h>
#include "objects.h"

#ifdef HAS_WINDOW
	#include <engine/display.h>
	#include "../client/components/progressBar.h"
	#include "../client/components/dialog.h"
	#include "../client/components/tutorial.h"
#endif

//#include <geometry/graph_mesh.h>
#include <geometry/cgs.h>
#include <geometry/converter/obj.h>
#include "stringTable.h"
#include "logEvent.h"
#include "runtimeConfig.h"



namespace Objects
{

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



	#ifdef CLIENT

		#ifdef THOROUGLY_DEBUG_SENSORY
			void	VerifySensory()
			{
				foreach (Grid::cellStack,stack)
					stack->ClearDbgSensory();
				foreach (structures,s)
					if ((*s)->isMine)
						Grid::InsertDbgSensor((*s)->location,(*s)->GetScanningRange());
				foreach (vehicles,s)
					if ((*s)->isMine)
						Grid::InsertDbgSensor((*s)->location,(*s)->GetScanningRange());
				foreach (Grid::cellStack,stack)
					stack->CheckDbgSensory();

			}
		#endif




	#else

	











	#endif


	#ifdef HAS_WINDOW

		/**
		Relay-functor to convert a single boolean function or variable to BaseCommand::Flags.
		Depending on the returned boolean value, flag compositions will either be 
		BaseCommand::Flags::IsEnabled | BaseCommand::Flags::IsPermaActive 
		or
		BaseCommand::Flags::IsEnabled
		*/
		class GlobalPermaSwitch
		{
			std::function<bool()>	getState;
		public:
			/**/	GlobalPermaSwitch(const std::function<bool()>&f):getState(f)
			{}
			/**/	GlobalPermaSwitch(bool&b):getState([&b](){return b;})
			{}


			BaseCommand::Flags	operator()(int, bool)
			{
				return getState() ? BaseCommand::Flags::IsEnabled | BaseCommand::Flags::IsPermaActive : BaseCommand::Flags::IsEnabled;
			}


		};



	#endif


	void					Create()
	{
		#ifdef CLIENT
			Grid::visibleOrbitGrid.orbit = Grid::ToRadius(Satellite::BaseResidenceHeight);
			Grid::visibleOrbitGrid.Clear();
			#ifdef HAS_WINDOW
				Renderer::Create();
			#endif
		#else

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
				#ifndef CONNECTOR_LESS
					<< PVehicleJob(new VehicleJob::TransferJob())
				#endif
				<< PVehicleJob(new VehicleJob::RepairJob())
				<< PVehicleJob(new VehicleJob::ClearAbove())
				<< PVehicleJob(new VehicleJob::DigHorizontal())
				<< PVehicleJob(new VehicleJob::DigDown())
				<< PVehicleJob(new VehicleJob::Construct())
				<< PVehicleJob(new VehicleJob::Sequence())
				;

		#endif
		LogEvent(EventSignificance::PerformanceMeasurement, nullptr, "Vehicle grid of "+String(vehicleGrid.GetContentSize())+" bytes allocated");
		LogEvent(EventSignificance::PerformanceMeasurement, nullptr, "Structure grid of "+String(structureGrid.GetContentSize())+" bytes allocated");




		structurePrototypes << Mine::globalPrototype;
		structurePrototypes << Generator::globalPrototype;
		structurePrototypes << ReactorCore::globalPrototype;
		structurePrototypes << MunitionFactory::globalPrototype;
		structurePrototypes << VehiclePlant::globalPrototype;
		structurePrototypes << Connector::globalPrototype;
		structurePrototypes << FuelContainer::globalPrototype;
		structurePrototypes << MineralContainer::globalPrototype;
		structurePrototypes << MunitionStorage::globalPrototype; //DO NOT COMMENT. Nexus bases feature these
		structurePrototypes << UraniumStorage::globalPrototype;
		structurePrototypes << NexusPort::globalPrototype;
		structurePrototypes << VehicleStore::globalPrototype;
		structurePrototypes << SkyPort::globalPrototype;

		satellitePrototypes << SkyHub::globalPrototype;
		satellitePrototypes << SkyGuardian::globalPrototype;
		satellitePrototypes << Ragnarok::globalPrototype;
		//satellitePrototypes << StarPort::globalPrototype;


		Tower::globalPrototypes.Append().reset(new Tower(&Component::launcher));
		//Tower::globalPrototypes.Append().reset(new Tower(artillery));
		Tower::globalPrototypes.Append().reset(new Tower(&Component::radar));
		Tower::globalPrototypes.Append().reset(new Tower(&Component::armored));
		structurePrototypes.Append(Tower::globalPrototypes);



		#ifdef HAS_WINDOW
		{
			LogEvent(EventSignificance::Debug, nullptr, "Loading common geometry");

			{
				Geometry geo;
				geo.LoadFromObj("objects/cordon.obj");

				geo.Scale(vehicleScale);
				(DX11::BaseInstructionBlock&)cordon = stdVehicleShader;
				cordon.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
				cordon.ibo.Create(geo.GetLOD(0).indices);

				geo.Clear();
				geo.LoadFromObj("objects/foundation.obj");
				//geo.Scale(vehicleScale);
				(DX11::BaseInstructionBlock&)foundation = stdVehicleShader;
				foundation.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
				foundation.ibo.Create(geo.GetLOD(0).indices);
			}
			{
				Geometry geo;
				static const int Resolution = 50;
				static const float r0 = 1,
									r1 = 2;
				for (int i = 0; i < Resolution; i++)
				{
					float fi = float(i) / float(Resolution);
					float a = fi * M_PIF * 2.f;
					float x = cos(a);
					float y = sin(a);

					geo.MakeVertex(float3(x*r0,y*r0,0.1),float3(0,0,1),float3(1,0,0));
					geo.MakeVertex(float3(x*r1,y*r1,0.1),float3(0,0,1),float3(1,0,0));

					geo.MakeQuad(i*2,i*2+1,((i+1)%Resolution)*2+1,((i+1)%Resolution)*2);

				}
				(DX11::BaseInstructionBlock&)userInteractionNotifier = stdVehicleShader;
				userInteractionNotifier.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
				userInteractionNotifier.ibo.Create(geo.GetLOD(0).indices);


				geo.LoadFromObj("objects/gatheringPoint.obj");

				(DX11::BaseInstructionBlock&)gatheringPoint = stdVehicleShader;
				gatheringPoint.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
				gatheringPoint.ibo.Create(geo.GetLOD(0).indices);

			}
			LogEvent(EventSignificance::Debug,nullptr,"Loading prototype geometry");
			foreach(satellitePrototypes, p)
				(*p)->PrototypeLoadGeometry();
			foreach(structurePrototypes, p)
				(*p)->PrototypeLoadGeometry();
		}
		#endif

		LogEvent(EventSignificance::Debug, nullptr, "Structure static setup");

		Structure::StaticSetup();

		Component::ConfigureAll();

		foreach(satellitePrototypes, p)
			(*p)->PrototypeSetup();
		foreach(structurePrototypes, p)
			(*p)->PrototypeSetup();

		

		#ifdef HAS_WINDOW
			foreach (Component::vehicleTurretComponents,tc)
				(*tc)->VisitAllCommands([](BaseCommand&cmd){cmd.RasterizeImage();});
			foreach (Component::baseComponents,bc)
				(*bc)->VisitAllCommands([](BaseCommand&cmd){cmd.RasterizeImage();});

			globalCommands.Append(GlobalCommand(int2(0,0),"End Round", [](int){if (HasSignalledRoundEnd()) SignalUndoRoundEnd(); else SignalRoundEnd();}, 
			[](int, bool controlEnabled)->BaseCommand::Flags
			{
				BaseCommand::Flags rs = BaseCommand::Flags::None;
				if (CanChangeRoundState())
					rs |= BaseCommand::Flags::IsEnabled;
				if (HasSignalledRoundEnd())
					rs |= BaseCommand::Flags::IsPermaActive;
				else
					if (ShouldSignalRoundEnd())
						rs |= BaseCommand::Flags::IsFlashing;
				return rs;
			}));


			globalCommands.Append(GlobalCommand(int2(1,2),"Clip Terrain", &GlobalCmd::ToggleSeeThrough
					,GlobalPermaSwitch(Grid::SeeThrough::IsEnabled)
				));
			globalCommands.Append(GlobalCommand(int2(2,2),"Clip-View Down", &GlobalCmd::SeeThroughDown
					,[](int,bool)->BaseCommand::Flags{return GlobalCmd::SeeThroughIsActive(0) ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;}
				));
			globalCommands.Append(GlobalCommand(int2(3,2),"Clip-View Up", &GlobalCmd::SeeThroughUp
					,[](int,bool)->BaseCommand::Flags{return GlobalCmd::SeeThroughIsActive(0) ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;}
				));

			globalCommands.Append(GlobalCommand(int2(2,0),"Hide Objects", &GlobalCmd::ToggleObjectView
					,GlobalPermaSwitch([](){return !objectView;})
				));


			globalCommands.Append(GlobalCommand(int2(0,1),"Show Resources", [](int)
					{
						resourceView = !resourceView;
						for (int j = 0; j < BaseGrid::NumFamilies; j++)
							Grid::low.family[j].normal.resourceOutdated = true;
			
					}
					,GlobalPermaSwitch(resourceView)
				));

			globalCommands.Append(GlobalCommand(int2(1,1),"Show Ranges", [](int)
					{
						showFriendlyRanges = !showFriendlyRanges;
					}
					, GlobalPermaSwitch(showFriendlyRanges)
				));
			globalCommands.Append(GlobalCommand(int2(2,1),"Show Hostile Ranges", [](int)
					{
						showHostileRanges = !showHostileRanges;
					}
					,GlobalPermaSwitch(showHostileRanges)
				));

			//ASSERT__(Grid::Resource::all.IsNotEmpty());	//wrong order if this fires
			//for (index_t i = 0; i < Grid::Resource::all.Count(); i++)
			//{
			//	const Grid::Resource&res = Grid::Resource::all[i];
			//	globalCommands.Append(GlobalCommand(int2(i,1),"Show "+res.GetName(), [i]()
			//	{
			//		if (i == activeResource)
			//		{
			//			resourceView = !resourceView;
			//			return;
			//		}
			//		for (int j = 0; j < BaseGrid::NumFamilies; j++)
			//			Grid::low.family[j].normal.resourceOutdated = true;
			//		resourceView = true;
			//		activeResource = i;

			//	}, &AlwaysTrueV, [i]()
			//	{
			//		return resourceView && activeResource == i;
			//	}));
			//}
		#endif
		#ifdef HAS_WINDOW
			RefreshCommands(false);
		#endif


		InitGrid();
		//globalCommands.Append(Command("Clip Terrain", &Cmd::ToggleSeeThrough, &AlwaysTrue, &Cmd::SeeThroughIsActive));
	}






	void					Destroy()
	{
		#ifdef HAS_WINDOW
			Renderer::Destroy();
			skyLinks.Clear();
		//	hexGrid.Clear();
		#endif
		structures.Clear();
		structurePrototypes.Clear();
		Component::DestroyAll();


	}
	
	#ifdef VOXEL_SCANNER
		class RayWalker
		{
		public:	
			TVec3<> atPos;
			const TVec3<> toPos;
			Grid::LCellIdI atCell;
			const Grid::LCellIdI toCell;
			count_t cellsTouched;
			bool	shaded;

			/**/	RayWalker(const Grid::LCellIdI&from, const Grid::LCellIdI&to, const TVec3<>&fromLocation, const TVec3<>&toLocation):
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
						const TVec3<> remote = Grid::low.cells[id.stackID.centerVertex.xyz * Grid::ToRadius(id.height);
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
						const TVec3<> remote = Grid::low.cells[id.stackID.centerVertex.xyz * Grid::ToRadius(id.height);
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

		bool	SphereEdgeDetection(const TVec3<>&center, float radius, const TVec3<>&point0, const TVec3<>&point1 )
		{
			return Obj::DetectSphereEdgeIntersection(center,radius,point0,point1);
		
		}
	

		#ifdef HAS_WINDOW
			PWorldEntity					GetRayIntersection(const TVec3<>&b, const TVec3<>&d, float&distance)
			{
				PWorldEntity newSel;
				foreach(vehicles, v)
				{
					if ((*v)->TestIntersection(b, d, distance))
					{
						newSel = *v;
					}
				}
				foreach(satellites, v)
				{
					if ((*v)->TestIntersection(b, d, distance))
					{
						newSel = *v;
					}
				}
				foreach(structures, s)
				{
					if ((*s)->TestIntersection(b, d, distance))
					{
						newSel = *s;
					}
				}
				foreach(Marker::mobileGhosts, v)
				{
					if ((*v)->TestIntersection(b, d, distance))
					{
						newSel = *v;
					}
				}
				foreach(Marker::structureGhosts, s)
				{
					if ((*s)->TestIntersection(b, d, distance))
					{
						newSel = *s;
					}
				}
				return newSel;
			}

			bool					Select(const TVec3<>&b, const TVec3<>&d, float&distance,bool controlEnabled)
			{
				//if (Compound::Transfer::isActive)
					//return false;

				//selectedEntity.reset();
				PWorldEntity newSel = GetRayIntersection(b,d,distance);

				//bool success = !!selectedEntity;
				if (newSel)
				{
					Select(newSel,controlEnabled);
				}
				#ifdef HAS_WINDOW
					else
						RefreshCommands(controlEnabled);	//in UpdateChoices() otherwise
				#endif
				return !!newSel;
			}

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
		MakeBox3(connectorMesh, Box<>(-0.7f, 0.f, 0.f, 0.7f, 1.f, 0.11f), float4(float3(0.25f), 1.f));
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


		TVec3<>			position,
		direction,
		up;				//!< Up axis. Must be normalized and orthogonal to @a direction
		TVec2<>			scale;
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
					const float scale = d.Length();
					const float3 d1 = d / scale;
					const float r0 = Grid::ToRadius(location.height);
					const float r1 = Grid::ToRadius(location.height + 1);

					const float3 x1 = -(b0 | d1).Normalized();
					const float3 x = x1 * (r1 - r0);
					for (index_t i = 0; i < numVertices; i++)
					{
						const Vertex&vtx = data.vertices[i];
						const TVec3<>&position = vtx.vertex;
						float r = r0 + (r1 - r0) * position.z;
						TVec3<> np = b0 + d * position.y + x * position.x / r;
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
					TVec3<> np, n;
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





	#ifdef HAS_WINDOW

		bool			DisableModes(bool controlEnabled)
		{
			if (activeCursorMode == CursorMode::Idle)
				return false;
			buildCandidates.Clear();
			removeCandidates.Clear();
			activeCursorMode = CursorMode::Idle;
			UpdateChoices(controlEnabled);
			return true;
		}



	#endif

	void			FlushAllData()
	{
		LogEvent(EventClass::Preset::uncommon,"Flushing scene data");

		#ifdef CLIENT	//temporary. once i figured out good values the limit will be restored from file and clearing is valid for server too
			RuntimeConfig::UnitLimits::Reset();
		#endif
		RuntimeConfig::hibernationSystemEnabled = false;

		storedMobileEntities.Clear();
		satellites.Clear();
		vehicles.Clear();
		structures.Clear();
		idMap.Clear();
		#ifndef CONNECTOR_LESS
			compounds.Clear();
		#endif
		vehicleGrid.Fill(PVehicle());
		structureGrid.Fill(PStructure());
		#ifdef CLIENT
			#ifdef HAS_WINDOW
				skyLinks.Clear();
			#endif
			Marker::FlushAll();
			#ifndef NO_HIBERNATION
				uplinkTraffic = 0;
				onlineUplinkTraffic = 0;
			#endif
			#ifdef HAS_WINDOW
				Deselect(false);
				resourceView = false;
				objectView = true;
				hasArrowCellTarget = false;
				showArrow = false;
				arrowFrom = float3();
				arrowFromCell = arrowCellTarget = Grid::LCellIdI(0,0);
				//activeResource = 0;

				cellSelection.Clear();
				buildCandidates.Clear();
				removeCandidates.Clear();
				UpdateChoices(false);
			#endif

			waste.Clear();

		#else
			Faction::all.Clear();
			Faction::RecreateNexus();
		#endif
	}



}

