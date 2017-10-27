#include <global_root.h>
#include "structure.h"
#include "systems.h"
#include "../cast.h"
#include "interface.h"
#include "../globalEvents.h"

#ifdef HAS_WINDOW
	#include "entityCommand.h"
	#include "interface.h"
#endif

#include "gridAndChoices.h"
#include "prototypes.h"
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
	SharedContainer<Structure>	structures;
	#ifdef CLIENT
		/**
		Filter list containing only SkyPorts. Used to faster render certain effects.
		*/
		Buffer0<TLinkReference>		skyLinks;
	#endif


	/*static*/ int				NexusPort::ResistenceRounds = 10;


	#ifdef HAS_WINDOW
		/*static*/ Grid::LCellIdI	GatheringPoint::selected;			//!< Gathering point of selected entity (if any, or invalid)
		/*static*/ M::TMatrix4<>		GatheringPoint::selectedVisual;	//!< Point where the respective gathering point should be visualized at. (same cell; may be different height)

		/*static*/ Geometry			Structure::standardConnector;

//		/*static*/ Buffer0<IconCommand>		Connector::commands;
		///*static*/ DX11::InstructionBlock32	Connector::shape;
		///*static*/ Geometry					Connector::geometry;




		
		//static IconCommand			cmdLeft,cmdRight;

		/*static*/ Geometry			Generator::reactorConnector;


	#endif





	/*static*/ count_t			Structure::CountOccurrences(const OwnerParameter&o,EntityType type)
	{
		count_t rs = 0;
		structures.Foreach([&rs,o,type](const Objects::PStructure&s)
		{
			if (s->owner == o && s->GetType() == type)
				rs++;
			else
			{
//				if (m->GetCTypeName() == type)
	//				ShowMessage(m->owner->GetName() + "!="+o->GetName());
			}
		});
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


		/*virtual*/ void				WorkingStructure::Tick(ResourceVector&v)
		{
			ResourceVector v1, s, m;
			GetResourceVector(v1,s,m);
			v += v1;
		}


	#endif


	Structure::Structure(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&at, bool buildSystem, float time) :Super(owner,at), table(table)
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

	bool			Structure::RemoveConnectionTo(const PStructure&s)
	{
		foreach (connectedTo,c)
			if (c->lock() == s)
			{
				connectedTo.erase(c);
				return true;
			}
		return false;
	}

	bool			Structure::RemoveConnectionTo(Structure*s)
	{
		foreach (connectedTo,c)
			if (c->lock().get() == s)
			{
				connectedTo.erase(c);
				return true;
			}
		return false;
	}


	bool			Structure::IsConnectedTo(const PStructure&s)	const
	{
		foreach (connectedTo,c)
			if (c->lock() == s)
				return true;
		return false;
	}

	bool			Structure::IsConnectedTo(const Structure*s) const
	{
		foreach (connectedTo,c)
			if (c->lock().get() == s)
				return true;
		return false;
	}







	void Structure::StaticSetup()
	{
		#ifdef HAS_WINDOW
			standardConnector.LoadFromObj("objects/standardConnector.obj");
		#endif
	}
	
	void	Structure::DisconnectFromNeighborhood(float&atTime)
	{
		for (index_t i = 0; i < this->connectedTo.Count(); i++)
		{
			PStructure c = this->connectedTo[i].lock();
			if (!c)
			{
				LogEvent(EventClass::Preset::error,this,"DisconnectFromNeighborhood(): Lost neighbor connection #"+String(i));
				continue;
			}
			c->SignalLostNeighbor(To<Structure>::Cast(shared_from_this()),atTime);
		}
		this->connectedTo.Clear();
		#ifdef HAS_WINDOW
			this->changed = true;
		#endif
	}


	void Structure::OnConstructionFinished(float&atTime)
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
					changed = c->changed = true;
				#endif
				c->OnNewConnection(_this, between, atTime);
				if (!c->IsConnectedTo(_this))
				{
					//this can happen when placing new structures and c is a nexus port that changes ownership as a result, and thus reconnects to all existing structures which this is not yet a part of
					DBG_ASSERT__(!IsConnectedTo(c));
					connectedTo << c;
					c->connectedTo << _this;
					c->OnNewConnection(_this, between, atTime);
					ASSERT__(c->IsConnectedTo(_this));
				}
				_this->OnNewConnection(c, between, atTime);
				LogEvent(EventClass::Preset::common,"Connection established between "+GetName()+" and "+c->GetName());

			}
		}
	}




	/*virtual*/ void Structure::OnDestroy(RemovalType t, float&atTime)
	{
		Super::OnDestroy(t,atTime);
		LogEvent(EventClass::Preset::debug,this,"Unregistering...");
		CommonUnregisterStructure(location,GetBroadHeight(),GetHeight(),HasFoundation(),atTime);

		LogEvent(EventClass::Preset::debug,this,"Disconnecting...");
		DisconnectFromNeighborhood(atTime);

		#ifdef HAS_WINDOW
			this->shape.reset();
			this->changed = false;
		#endif
	}








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


	Connector::Connector(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(table,owner,location, true, atTime)
	{}










	/*static*/ void Structure::ChangeStructCount(const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, int changeDirection, float atTime)
	{
		using StringConversion::ToString;
		LogEvent(EventSignificance::Debug,nullptr,"ChangeStructCount("+ToString(location)+", broadHeight="+String(broadHeight)+", height="+String(totalHeight)+", foundation="+String(hasFoundation)+", dir="+String(changeDirection)+")");


		{
			auto seq = World::actual.BeginChangeSequence(atTime);

			const int lowEdge = hasFoundation ? -1 : 0;
			#ifdef HYBRID_GRID
				const UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(higher).neighborhood;
				for (int j = 0; j < n.numNeighbors; j++)
				{
					auto acc = seq.AccessStack(n.neighborID[j]);
					Grid::CellStack&stack = acc.GetStack();
					for (int h = lowEdge; h < broadHeight; h++)
						stack.ChangeStructureCounterI(location.height+h,changeDirection);
				}
				auto acc = seq.AccessStack(higher);
				Grid::CellStack&stack = acc.GetStack();
			#else
				auto acc = seq.AccessStack(location.stackID);
				Grid::CellStack&stack = acc.GetStack();
			#endif
			for (int h = lowEdge; h < broadHeight; h++)
				stack.ChangeStructureCounterI(location.height+h,2*changeDirection);
			for (int h = broadHeight; h < totalHeight; h++)
				stack.ChangeStructureCounterI(location.height+h,changeDirection);
		}
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
					const Grid::CellStack&stack = World::actual.GetStack(n.neighborID[j]);
					if (stack.GetCellI(location.height).content != Grid::CellStack::Cell::Content::Clear)
						return GroundLevelObstruction;
					for (int h = 1; h < broadHeight; h++)
						if (stack.GetCellI(location.height + h).content != Grid::CellStack::Cell::Content::Clear)
							return UpperLevelObstruction;
				}
			}
			const Grid::CellStack&stack = World::actual.GetStack(higher);
		#else
			const Grid::CellStack&stack = World::actual.GetStack(location.stackID);
		#endif
		if (stack.GetCellI(location.height).content != Grid::CellStack::Cell::Content::Clear)
			return GroundLevelObstruction;
		for (int h = 1; h < totalHeight; h++)
			if (stack.GetCellI(location.height + h).content != Grid::CellStack::Cell::Content::Clear)
				return UpperLevelObstruction;

		return CanPlace;
	}

	int										Generator::GetEnergyProduction() const
	{
		PReactorCore c = connectedToReactor.lock(); 
		return c && c->IsEnabled() ? 40 : 20;
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
					changed =  true;
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
				core = To<ReactorCore>::DynamicCast(conn->lock());
				if (core && core->location.height == location.height)
				{
					connectedToReactor = core;
					#ifdef HAS_WINDOW
						changed = true;
					#endif
					return;
				}
			}
		}
	}


	

	



	
	/*virtual*/ void				Structure::SignalLostNeighbor(const PStructure&other, float&atTime)
	{
		atTime += timeEpsilon;
		if (RemoveConnectionTo(other))
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
				changed = true;
			#endif
		}

	}


	/*virtual*/ bool				Structure::SignalEnvironmentChange(const TStructureNeighborhood&neighborhood, float atTime)
	{
		#ifdef HYBRID_GRID
			const UINT32 high = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			const auto&n = Grid::high.grid.GetCell(high).neighborhood;
		#endif
		
		auto _this = To<Structure>::Cast(shared_from_this());
		count_t limit = connectedTo.Count();
		foreach(neighborhood, candidate)
			if (!IsConnectedTo(*candidate))
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
						changed = true;
					#endif
					OnNewConnection(c, between,atTime);
					c->connectedTo << _this;
					c->OnNewConnection(_this,between,atTime);
					LogEvent(EventSignificance::Common,nullptr, "Connection established between "+GetName()+" and "+c->GetName());

				}
			}

		for (index_t i = 0; i < limit; i++)
		{
			const PStructure c = connectedTo[i].lock();
			if (!c)
			{
				LogEvent(EventClass::Preset::error,this,"SignalEnvironmentChange(): Removing lost neighbor connection #"+String(i));
				connectedTo.Erase(i);
				i--;
				continue;
			}
			#ifdef HYBRID_GRID
				const UINT32 high2 = Grid::low.grid.GetCell((c)->location.stackID).higherEquivalent;
				const auto&n2 = Grid::high.grid.GetCell(high2).neighborhood;
				UINT32 between = BaseGrid::RequireIntersection(n, n2);
			#else
				UINT32 between = 0;
			#endif
			if (!ConnectionPossibleTo(c, between) || !c->ConnectionPossibleTo(_this,between))
			{
				c->RemoveConnectionTo(_this);
				connectedTo.Erase(i--);
				limit--;
				#ifdef HAS_WINDOW
					c->changed = changed = true;
				#endif
				OnConnectionLost(c, between,atTime);
				c->OnConnectionLost(_this,between,atTime);
				LogEvent(EventSignificance::Common,nullptr, "Connection lost between "+GetName()+" and "+c->GetName());

			}
		}
		return true;
	}



	#ifndef NO_SATELLITES
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

	#endif


	/*virtual*/ void				Structure::PrototypeSetup()
	{
		Super::PrototypeSetup();
	}



	#ifndef CLIENT

	
		#ifndef NO_SATELLITES
		/*virtual override*/ void			SkyPort::OnSleepChange(float&t)
		{
			Super::OnSleepChange(t);

				if (IsSleeping() && IsEnabled())
				{
					LogEvent(EventSignificance::Common,this,"Fell asleep. Disabling");
					SetEnabled(false,t);
				}
			}
		#endif

		#ifndef NO_SATELLITES
			/*virtual override*/ bool			SkyPort::DirectTo(CursorMode mode, Ctr::ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
			{
				if (waypoints.Count() != 1)
					return false;
				MobileEntityProducer::UpdateGatheringPoint(waypoints.First());
				return true;
			}
		#endif
	#endif


	
	#ifndef CLIENT





		#ifndef NO_SATELLITES
			/*virtual override*/ void			SkyPort::OnEnabledChange()
			{
				PSkyHub hub = connectedToHub.lock();
				if (!this->IsEnabled())
				{
					if (hub)
					{
						//LogEvent(EventClass::Preset::operationAborted,this,"Connection to "+connectedTo->GetNiceName()+" lost");
						hub->LoseGroundConnection();
					}
				}
				else
					if (!hub)
					{
						PSkyHub otherHub = To<SkyHub>::DynamicCast(GetSatellite(location.stackID));
						if (otherHub && otherHub->owner == owner)
						{
							if (otherHub->GainGroundConnection(To<SkyPort>::Cast(shared_from_this())))
								ASSERT__(otherHub == connectedToHub.lock());
						}
					}
			}


			/*virtual override*/ void				SkyPort::Tick(ResourceVector&v)
			{
				v.level[Resource::Energy] -= EnergyConsumption;
				if (MobileEntityProducer::queue.IsNotEmpty())
				{
					float t = 0;
					MobileEntityProducer::Tick(v,t);
				}



			}
		#endif


	



	#else
		




		#ifdef HAS_WINDOW




			#ifndef NO_SATELLITES
				/*virtual override*/ void			SkyPort::OnSelect()
				{
					gatheringPoint.UpdateVisuals();
				}
				/*virtual override*/ void			SkyPort::OnDeselect()
				{
					gatheringPoint.selected = Grid::LCellIdI::Invalid;
				}
			#endif
		#endif






	#endif



	/*virtual override*/ void				ReactorCore::PrototypeSetup()
	{
		Super::PrototypeSetup();
	}

	#ifdef HAS_WINDOW
		#ifndef NO_SATELLITES
			/*virtual override*/ void				SkyPort::VisitAllCommands(FCommandVisitor visitor, bool force)
			{
				MobileEntityProducer::VisitAllCommands(visitor,force);
				Super::VisitAllCommands(visitor,force);
			}
		#endif
	#endif

	#ifndef NO_SATELLITES
		/*virtual override*/ bool				SkyPort::AwaitsUserInteraction() const 
		{
			return MobileEntityProducer::AwaitsUserInteraction();
		}

		/*virtual override*/ bool				SkyPort::CanProduce(const PWorldEntity&e) const 
		{
			return (e->IsVehicle() && To<Vehicle>::Cast(e)->base == &Component::hoverbase && To<Vehicle>::Cast(e)->turret == &Component::terraformer)
				||
				(e->IsSatellite() && e->GetCTypeName() == ConstName::Type_SkyHub)
			;
		}
	#endif

	#ifndef NO_SATELLITES
		/*virtual override*/ void				SkyPort::PrototypeSetup()
		{
			Super::PrototypeSetup();
			ResearchState::RegisterUnlockCost(this,20);

			#ifdef HAS_WINDOW
	
				//vehicleConstructionCommands << VehicleConstructionCommand("Base:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);
				//foreach (baseComponents,comp)
				{
					//VehicleBase* c = *comp;
					commands.Add(new EntityConstructionCommand("Construct Hover-Former", &Component::hoverbase,&Component::terraformer, PWorldEntity(),[]()
					{
						PSkyPort port = To<SkyPort>::DynamicCast(selectedEntity);
						if (!port)
							return;
						SignalConstructionBegin(port->id, ConstName::VehicleBase_HoverBase, ConstName::Turret_Terraformer);
					}
					,CommonCmd::CommonButtonCheck));
					commands.GetLast().second->RasterizeImage();

					commands.Add(new EntityConstructionCommand("Construct SkyHub", nullptr,nullptr, SkyHub::globalPrototype, []()
					{
						PSkyPort port = To<SkyPort>::DynamicCast(selectedEntity);
						if (!port)
							return;
						SignalConstructionBegin(port->id, ConstName::Type_SkyHub, ConstName::Empty);
					}
					,
					[](bool controlsEnabled)
					{
						if (!controlsEnabled)
							return Objects::CommandFlags::None;
						return selectedEntity && !GetSatellite(selectedEntity->location.stackID) ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;
					}));
					commands.GetLast().second->RasterizeImage();
				}
			#endif

			//commands << Command("Queue:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);

		}
	#endif


	
	/*virtual override*/ bool				NexusPort::ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)
	{
		#ifndef CLIENT
			if (connectedTo.IsEmpty())
				return true;
			PStructure s = connectedTo.First().lock();
			ASSERT__(s);
			return other->owner == s->owner;
		#else
			return true;
		#endif
	}

	/*virtual override*/ void				NexusPort::OnNewConnection(const PStructure&other, UINT32 acrossHighCell, float atTime)
	{
		#ifndef CLIENT
			Super::OnNewConnection(other,acrossHighCell,atTime);
			if (owner != other->owner && !RuntimeConfig::loadingPhase)
			{
				resistence = ResistenceRounds;
				SetEnabled(false,atTime,false);
				ChangeOwnership(shared_from_this(), other->owner,atTime);
			}
		#endif
	}

	/*virtual override*/ void				NexusPort::OnConnectionLost(const PStructure&other, UINT32 acrossHighCell, float atTime)
	{
		#ifndef CLIENT
			Super::OnConnectionLost(other,acrossHighCell,atTime);
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
			if (owner != Faction::nexus)
			{
				if (resistence > 0)
					owner->acquiringNexusControl++;
				else
					owner->nexusControl++;
			}
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
					ApplicationInterface::SignalNexusControlChange(owner);
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
					ApplicationInterface::SignalNexusControlChange(owner);
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
					{
						PStructure st = c->lock();
						if (st)
						{
							if (st->compoundID == Compound::InvalidNetwork)
								primary << st.get();
						}
					}
				}
				network.Compact();
			}

		#else
			#ifdef CLIENT

				for (const auto&faction : Faction::allKnown)
				{
					faction->compound.allStructures.Clear();
					faction->compound.workingStructures.Clear();
				}

				structures.Foreach([](const Objects::PStructure&s)
				{
					s->owner->compound.AppendStructure(s.get());
				});
			#else
//				compounds.SetSize(Faction::all...)
				for (const auto&f : Faction::all)
				{
					f->UpdateCompoundCoverage();
				}

				Faction::nexus->UpdateCompoundCoverage();

			#endif


		#endif

		RefreshCompoundStates(atTime);
		GlobalEvents::invalidateObjectCache();
	}



	#ifdef HAS_WINDOW



		/*virtual override*/ void			NexusPort::DescribeType(Command::Base::Label&label, UINT32 descFlags)
		{
			label.BeginBrief();
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label << "Nexus Ports are indestructible structures of unknown origin."
			" Depending on game-settings, non-player Nexus Ports serve as spawning-beakons for Nexus units."
			" Nexus Ports will shift ownership to the respective owner of the first connected player building."
			" They must be enabled for a number of rounds in order to count towards winning the game."
			" Disabling a Nexus Port will make it stop counting as such."
			" A Nexus Port that loses all connections to other player-owned structures will shift back to"
			" the Nexus faction."
			;
		}



		/*virtual override*/ void			Generator::DescribeType(Command::Base::Label&label, UINT32 descFlags)
		{
			label.BeginBrief();
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label << "Enabled generators provide energy for the local compound."
			" Since many structures consume "+String(ResourceTag(Resource::Energy))+" when enabled, generators are a crucial component of any active base."
			" Combine with reactor cores for increased output."
			;
		}

		/*virtual override*/ void			ReactorCore::DescribeType(Command::Base::Label&label, UINT32 descFlags)
		{
			label.BeginBrief();
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label  << "Amplifies the energy output of connected generators. Only one reactor core can be connected to any one generator,"
			" but multiple generators may be connected to the same reactor core."
			" Must be placed on the same height level as the generator(s) to connect to."
			;
		}


		/*virtual override*/ void			WorkingStructure::DescribeType(Command::Base::Label&label, UINT32 descFlags)
		{
			label.BeginBrief();
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
							label.GetLastLine() += " "+String(ResourceTag(i))+String(-balance.level[i]);
						}
				}
				if (balance.SumPositive())
				{
					label << "Produces:";
					for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
						if (balance.level[i] > 0)
						{
							label.GetLastLine() += " "+String(ResourceTag(i))+String(balance.level[i]);
						}
				}
			}
			if (maxStorage.IsNotEmpty())
			{
				label << "Storage capacity:";
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					if (maxStorage.level[i] > 0)
					{
						label.GetLastLine() += " "+String(ResourceTag(i))+String(maxStorage.level[i]);
					}
			}
		
		}



	#endif


	
	bool						WorkingStructure::CouldEnable(bool ignoreCanEnable/*=false*/)
	{
		if (IsDeadOrGhost())
			return false;
		if (IsEnabled())
			return true;

		const Compound&n = GetCompound();
		return n.CanEnable(this,nullptr,nullptr,ignoreCanEnable);
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


		bool						WorkingStructure::SetEnabled(bool b, float&atTime, bool refreshNetwork/*=true*/, force_t force/*=EnableIfPossible*/, bool signalStructure/*=true*/)
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
					Ctr::Array<Resource> missingResources;
					if (!network.CanEnable(this,&outWhy,&missingResources))
					{
						LogEvent(EventClass::Preset::operationAborted,this,"Cannot enable"+GetTypeName()+": Insufficient resource supply in compound ("+outWhy+")");
						if (missingResources.IsNotEmpty())
						{
							atTime += timeEpsilon;
							ApplicationInterface::SignalOperationLacksResources(*this,missingResources,atTime);
						}
						return false;
					}
				}
				//else
				//{
				//	LogEvent(EventClass::Preset::error,this,"Compound ID out of bounds: "+String(compoundID)+"/"+String(compounds.Count()));
				//	return false;
				//}
			}
			atTime += timeEpsilon;
			if (!b)
				Compound::lastDisabled = this;
			else
			{
				Compound::lastDisabled = nullptr;
				SetSleep(false,atTime,false);
			}
			atTime = entityTime = std::max(entityTime,atTime)+timeEpsilon;
			LogEvent(EventClass::Preset::common,this,"Enabled-state changed "+String(isEnabled)+"->"+String(b)+" at t="+String(atTime)+", given signalStructure="+String(signalStructure)+", refreshNetwork="+String(refreshNetwork));
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
