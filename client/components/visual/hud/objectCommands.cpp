#ifdef HAS_WINDOW

#include <global_root.h>
#include "objectCommands.h"
#include "../scene/objectRendering.h"
#include "../../../ExtRenderer/DirectX11.h"
#include <engine/display.h>
#include <objects/vehicle.h>
#include <objects/structure.h>
#include "../objectSelection.h"
#include "../objectChoices.h"
#include <objects/structure/tower.h>
#include <objects/satellite.h>
#include <objects/faction.h>
#include <objects/gridAndChoices.h>
#include <objects/components.h>
#include <image/converter/magic.h>
#include <objects/research.h>
#include "../../network.h"
#include "../scene/terrain.h"



extern Engine::Display<Engine::DirectX11>	display;
//extern Engine::VectorCamera<>				scene;

namespace Objects
{
	Command::Container		globalCommands;
	Command::PCommandGroup	viewCommands;

	namespace Command
	{

		namespace Net
		{
			void QueueResearch(const Objects::Research::PTopic&topic, bool drop)
			{
				#ifdef HAS_WINDOW
					Network::Channels::queueResearch.SendTo(Network::client,Protocol::QueueResearch((UINT32)topic->serialIndex,drop));
					if (Objects::selectedEntity && Objects::selectedEntity->IsWorkingStructure() && !To<Objects::WorkingStructure>::Cast( Objects::selectedEntity)->IsEnabled())
					{
						Network::Channels::enableStructure.SendTo(Network::client,Protocol::EnableStructure(Objects::selectedEntity->id,true));
					}
				#endif
			}
		}


		template <bool ControlEnabled>
			void RefreshCommand(const Command::PBase&cmd)
			{
				cmd->Refresh(ControlEnabled);
			}


		/*static*/ Buffer0<WCommandGroup>		CommandGroup::activeGroupPath;


		/*static*/ HashTable<PathString, DX11::Texture>	VisualCommand::textureMap;



	

		float4 BackgroundColor(const PWorldEntity&e)
		{
			return float4(e->IsAlive() && e->owner ? e->owner->activeEntityColor*0.4f : float3(),0.5f);

		}

		/**/			ResearchCommand::ResearchCommand(const Research::PTopic&topic) : Super(topic->name,topic->iconName),topic(topic)	{}

		///*virtual override*/ void	ResearchCommand::LoadIcon()
		//{
		//	if (topic->iconName.IsNotEmpty())
		//		Super::LoadIcon(topic->iconName);
		//}

		/*virtual override*/ void	ResearchCommand::Execute()
		{
			Super::Execute();
			if (selectedEntity && selectedEntity->owner &&  topic->CanResearch(selectedEntity->owner))
			{
				Net::QueueResearch(topic,false);
				this->Refresh(true);
			}
		}

		/*virtual override*/ void ResearchCommand::RasterizeImage()
		{
			PWorldEntity e = topic->prototypeReference;
			if (e)
				EntityCommand::UpdateTexture(*this,e,true);
			elif (topic->componentReference)
				if (topic->componentReference->IsVehicleBase())
					EntityConstructionCommand::UpdateTexture(*this,(Component::VehicleBase*)topic->componentReference,nullptr);
				elif (topic->componentReference->IsTurret())
					EntityConstructionCommand::UpdateTexture(*this,nullptr,(Component::Turret*)topic->componentReference);
		}


		/*virtual override*/	void	ResearchAbortCommand::Refresh(bool controlEnabled)
		{
			OwnerParameter owner = selectedEntity ? selectedEntity->owner : OwnerParameter();
			if (topic->IsGroup())
			{
				Super::Refresh(controlEnabled);
			}
			else
			{
				state = Objects::CommandFlags::None;
				if (isActive)
					state |= Objects::CommandFlags::IsPermaActive;
				if (!controlEnabled)
					return;
				state |= Objects::CommandFlags::IsEnabled;
			}
		}

		/*virtual override*/ void	ResearchAbortCommand::Execute()
		{
			if (selectedEntity && selectedEntity->owner == Faction::mine)
			{
				Net::QueueResearch(topic,true);
				//this->Refresh(true);
			}
		}


		/*virtual override*/	void	ResearchCommand::Refresh(bool controlEnabled)
		{
			OwnerParameter owner = selectedEntity ? selectedEntity->owner : OwnerParameter();
			if (topic->IsGroup())
			{
				Super::Refresh(controlEnabled);
			}
			else
			{
				state = Objects::CommandFlags::None;
				//if (owner && owner->researchState.GetActiveTopic() == topic)
				//	state |= Objects::CommandFlags::IsPermaActive;
				if (!controlEnabled || !topic->CanResearch(owner))
					return;
				state |= Objects::CommandFlags::IsEnabled;
			}
		}


		/*virtual override*/ void		ResearchCommand::CompileLabel(Label&outResult)
		{
			outResult.BeginBrief();

			if (topic->IsGroup())
			{
				outResult << topic->name;
				return;
			}
			const auto* state = selectedEntity && selectedEntity->owner ? &selectedEntity->owner->researchState.topicProgressVector : nullptr;

			int current = 0;
			int max = topic->maxProgress;
			if (state)
			{
				current = state->at(topic->serialIndex);
			}
	//		const Research::FactionState*rs = selectedEntity && selectedEntity->owner ? &selectedEntity->owner->researchState : nullptr;

			//if (ref || (base && turret))
			{
				outResult << topic->GetFullName();
				String dependentOn;
				//else
				{
					if (max < 0)
						outResult << "Research progress "+String(current);
					elif (current < max)
					{
						outResult << "Research progress "+String(current) + "/"+String(max);
					}
					else
						outResult << "Research completed";
				}
				Research::PTopic dep = topic->dependency.lock();
				if (dep && (!state || state->at(dep->serialIndex) < dep->maxProgress))
					outResult << "Requires completion of "+dep->GetFullName()+" research";
				outResult.MarkBriefEnd(true);
			}

			PWorldEntity ref = topic->prototypeReference;
			if (ref)
			{
				ref->DescribeType(outResult,WorldEntity::DescIncludeMaxHealth);
				return;
			}

			if (topic->componentReference)
			{
				if (topic->componentReference->IsVehicleBase())
				{
					((Component::VehicleBase*)topic->componentReference)->DescribeType(outResult,false,true,selectedEntity ? selectedEntity->owner : OwnerParameter());
				}			
				elif (topic->componentReference->IsTurret())
				{
					auto turret = ((Component::Turret*)topic->componentReference);
					turret->DescribeType(outResult,false, true,turret->mountType.ToString(),selectedEntity ? selectedEntity->owner : OwnerParameter());
				}
			}
		}


		static void SetPrototypeKeyColor()
		{
			keyColor.Update(float4(0.5,0.5,1,1));
		}



		/*virtual override*/ void		EntityConstructionCommand::GetCosts( int&costOut, Resource&costTypeOut, int&timeCostOut )
		{
			if (getCosts)
				getCosts(costOut,costTypeOut,timeCostOut);
			else 
			{
				PWorldEntity e = Super::reference.lock();
				if (e)
				{
					costOut = e->GetConstructionCost();
					timeCostOut = 0;
					costTypeOut = Resource::Minerals;
				}
				else if (turret && base)
				{
					const auto&state = Objects::selectedEntity->GetResearchState();
					costOut = Vehicle::GetPrice(*base,*turret,state);
					timeCostOut = 0;
					costTypeOut = Resource::Minerals;
				}
				else
					Super::GetCosts(costOut,costTypeOut,timeCostOut);
			}
		}

		/*static*/ void		EntityConstructionCommand::UpdateTexture(VisualCommand&target, Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor/* = float4(0)*/)
		{
			float distanceFactor = 1.f;

			GeometryImage::BeginRendering(backgroundColor,distanceFactor);

			if (!base || !turret)
				SetPrototypeKeyColor();

			M::TMatrix4<> sys = M::Matrix<>::eye4;
			sys.x.x = sys.y.y = sys.z.z = 2.5f;
			
			display.SetObjectMatrix(sys);
			if (base)
				Renderer::RenderComponent(sys,0,*base,0,0);
			if (turret)
				Renderer::RenderComponent(sys,base ? base->height : 0,*turret,0,0);


			target.texture = GeometryImage::EndRendering();

		}

		static void UpdateStateColors(const PWorldEntity&v, const float4&tone, bool isPrototype)
		{
			if (isPrototype)
			{
				SetPrototypeKeyColor();
				return;
			}
			if (v->IsDeadOrGhost())
				overallTint.Update(float4(0.25f));

			if (v->IsDeadOrGhost())
				keyColor.Update(float4(0,0,0,1));
			else
				keyColor.Update(float4(v->owner->activeEntityColor,1));
			#ifndef NO_HIBERNATION
				if (v->ToHibernating()->IsHibernating())
				{
					if (v->isOwnedByMe)
						keyColor.Update(float4(0,0,0,1));
					else
						keyColor.Update(float4(tone.rgb*0.5f,1));
				}
			#endif
			//else
		}


		void				EntityConstructionCommand::UpdateTexture(Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor /*= float4(0)*/)
		{
			UpdateTexture(*this,base,turret,backgroundColor);
	
		}




		/*static*/ void		EntityCommand::UpdateTexture(VisualCommand&target, const PWorldEntity&e, bool isPrototype, const float4&tone/* = float4(0)*/)
		{
			float distanceFactor = 1.f;
			if (e->IsMobile())
			{
				distanceFactor *= 0.5f;
			}
			if (e->IsStructure())
				distanceFactor += 0.1f * To<Structure>::Cast(e)->GetHeight();
			#ifndef NO_SATELLITES
				if (e->IsSatellite())
					distanceFactor *= 3.f;
			#endif
			GeometryImage::BeginRendering(isPrototype ? tone : BackgroundColor(e),distanceFactor);

			UpdateStateColors(e,tone,isPrototype);

			Renderer::RenderSimpleIdentity(*e);

			target.texture = GeometryImage::EndRendering();
		}


		void				EntityCommand::UpdateTexture(const PWorldEntity&v, bool isPrototype, const float4&backgroundColor/* = float4(0)*/)
		{
			UpdateTexture(*this,v,isPrototype, backgroundColor);
		}





		/*virtual override*/ void		EntityConstructionCommand::CompileLabel(Label&outResult)
		{
			outResult.BeginBrief();
			const MobileEntityProducer*p = selectedEntity ? selectedEntity->CastToProducer() : nullptr;
			//PWorkingStructure ws = To<WorkingStructure>::DynamicCast(e);

			PWorldEntity ref = reference.lock();
			int rawPrice = ref ? ref->GetConstructionCost() : Vehicle::GetPrice(base,turret,selectedEntity->GetResearchState());
			int price = (int)std::min(mineralsLeft, (unsigned)rawPrice);

			//if (ref || (base && turret))
			{
				UINT32 rounds = 0,rawRounds = 0;

				if (p)
					rawRounds = (unsigned)p->ProductionRounds(rawPrice,false);
				else
				{
					ASSERT__(selectedMobileEntity && selectedMobileEntity->turret->CanConstruct());
					rawRounds = selectedMobileEntity->GetOperationCompletionRounds(rawPrice,Objects::Component::Construct);
				}
				rounds = (int)std::min(roundsLeft, rawRounds);

				outResult << name;
				outResult << String(ResourceTag(Resource::Minerals))+ " "+String(price);
				if (price != rawPrice)
					outResult.GetLastLine() += "/"+String(rawPrice);
				if (rounds)
				{
					outResult.GetLastLine() += "  "+String(TimeResourceTag()) + String(rounds);
					if (rounds != rawRounds)
						outResult.GetLastLine() += "/"+String(rawRounds);
				}
			}

			if (ref)
			{
				ref->DescribeType(outResult,WorldEntity::DescIncludeMaxHealth|WorldEntity::DescIncludeUnitCount);
				return;
			}

			if (base && turret)
			{

				PVehicle v(new Vehicle(selectedEntity->owner, selectedEntity->location, base,turret));
				v->DescribeType(outResult,WorldEntity::DescIncludeMaxHealth|WorldEntity::DescIncludeUnitCount);
			}
			else
			{
				if (base)
				{
					//PVehicle v(new Vehicle(p->owner, p->location, base,turret));
					base->DescribeType(outResult,false,true,OwnerParameter());
				}			
				if (turret)
					turret->DescribeType(outResult,false, true,"Vehicle",selectedEntity->owner);

			}
		}






		/**/				DeployMobileCommand::DeployMobileCommand(const PMobileEntity&v,   FOnExecute f, FCheck c) : Super(v->GetNiceName(),v,f,c)	{}



		/*virtual override*/ void		DeployMobileCommand::CompileLabel(Label&outResult)
		{
			outResult.BeginBrief();
			PHangar h = To<Hangar>::DynamicCast(selectedEntity);
			ASSERT__(h);
			PMobileEntity vehicle = ToMobileEntity( this->reference.lock() );
			if (vehicle)
			{
				outResult << "Deploy mobile unit: "+name;

				vehicle->DescribeType(outResult,Objects::WorldEntity::DescIncludeCurrentHealth | Objects::WorldEntity::DescIncludeMaxHealth | WorldEntity::DescIncludeUnitCount);
			}
		}



		/*virtual*/ void		VisualCommand::CompileLabel(Label&outResult)
		{
			outResult.BeginBrief();
			int resourceCost, timeCost;
			Resource resourceType;
			GetCosts(resourceCost,resourceType,timeCost);

			if (timeCost == 0)
			{
				outResult << name;
				return;
			}

			outResult << name;
			if (resourceCost > 0)
			{
				outResult << " "+String(ResourceTag(resourceType))+ " "+String(resourceCost) + "  ";
			}
			else
				outResult<<" ";
			//outResult << "Rounds: "+String(timeCost);
			outResult.GetLastLine() += TimeResourceTag() + String(timeCost);
		}

		void			VisualCommand::LoadIcon()
		{
			LoadIconByName(name);
		}

		static bool NotValidFSChar(char c)
		{
			if (c >= 'a' && c <= 'z')
				return false;
			if (c >= 'A' && c <= 'Z')
				return false;
			if (c >= '0' && c <= '9')
				return false;
			return c != '_';
		}

		void			VisualCommand::LoadIconByName(const String&name)
		{
			String name2 = name;
			name2.FindAndReplace(NotValidFSChar,'-');

			PathString filename = PathString("resources/icons/"+name2+".png");
			if (!FileSystem::IsFile(filename))
				filename = PathString("resources/icons/Question-Mark.png");
			if (textureMap.Query(filename,texture))
				return;
			try
			{
				Image image;
				Magic::LoadFromFile(image,filename);
				if (image.channels() < 4)
				{
					image.SetChannels(4,BaseImage::CA_REPEAT);

				}
				else
				{
						
					Concurrency::parallel_for(UINT32(0),image.GetWidth(),[&image](UINT32 x)
					{
						for (UINT32 y = 0; y < image.GetHeight(); y++)
						{
							BYTE*pixel = image.get(x,y);
							Vec::mul(Vec::ref3(pixel),float(pixel[3])/255.f);
						}
					});

				}

				image.FlipVertically();
				texture.Load(image);
				textureMap.Set(filename,texture);
			}
			catch (const std::exception&e)
			{
				FATAL__(e.what());
			}
		}

		/*static*/ index_t	CommandGroup::FindInPath(const CommandGroup*g, const TCodeLocation&loc)
		{
			for (index_t i = 0; i < activeGroupPath.Count(); i++)
			{
				auto locked = activeGroupPath[i].lock();
				if (!locked)
				{
					LogEvent(EventClass::Preset::internalError,"Active group path contains NULL element at #"+String(i)+"/"+String(activeGroupPath.Count())+". Called by "+String(loc));
					activeGroupPath.Erase(i--);
					DBG_FATAL__("Active Group Path Violation");
					continue;
				}
				if (locked.get() == g)
					return i;
			}
			return InvalidIndex;
		}

		/*static*/ index_t	CommandGroup::FindInPath(const CommandContainer*c, const TCodeLocation&loc)
		{
			for (index_t i = 0; i < activeGroupPath.Count(); i++)
			{
				auto locked = activeGroupPath[i].lock();
				if (!locked)
				{
					LogEvent(EventClass::Preset::internalError,"Active group path contains NULL element at #"+String(i)+"/"+String(activeGroupPath.Count())+". Called by "+String(loc));
					activeGroupPath.Erase(i--);
					DBG_FATAL__("Active Group Path Violation");
					continue;
				}
				if (locked.get() == c)
					return i;
			}
			return InvalidIndex;
		}

		void			CommandGroup::CollapseChildren()
		{
			if (FindInPath(this,CLOCATION) != InvalidIndex)
				return;
			while (activeGroupPath.IsNotEmpty())
			{
				auto last = activeGroupPath.Last().lock();
				if (!last)
				{
					LogEvent(EventClass::Preset::internalError,"Active group path contains NULL element, found while collapsing children of "+name);
					activeGroupPath.EraseLast();
					DBG_FATAL__("Active Group Path Violation");
					continue;
				}
				if (last.get() == this)
					break;
				last->Collapse();
				activeGroupPath.EraseLast();
			}
			Collapse();
		}

		/*static*/ void		CommandGroup::CollapseAll()
		{
			for (index_t i = 0; i < activeGroupPath.Count(); i++)
				SafeCollapse(i,CLOCATION);
			activeGroupPath.Clear();
		}

		/*static*/ void CommandGroup::SafeCollapse(index_t at, const TCodeLocation&loc)
		{
			auto c = activeGroupPath[at].lock();
			if (c)
				c->Collapse();
			else
			{
				LogEvent(EventClass::Preset::internalError,"Active group path contains NULL element at #"+String(at)+"/"+String(activeGroupPath.Count())+". Caller: "+String(loc));
				DBG_FATAL__("Active Group Path Violation");
			}
		}
	
		/*virtual override*/ void	CommandGroup::Execute()
		{
			bool doHide = state & Flags::IsPermaActive;

			//ASSERT__(activeGroupPath.IsEmpty() || activeGroupPath.Contains(this));

			index_t at = FindInPath(this,CLOCATION);
			if (at != InvalidIndex)
			{
				for (index_t i = at+1; i < activeGroupPath.Count(); i++)
					SafeCollapse(i,CLOCATION);
				if (doHide)
					SafeCollapse(at,CLOCATION);
				activeGroupPath.Truncate(at+1);
				ASSERT_EQUAL__(at+1,activeGroupPath.Count());
			}
			else
			{
				at = FindInPath(parent,CLOCATION);
				if (at == InvalidIndex)
				{
					for (index_t i = 0; i < activeGroupPath.Count(); i++)
						SafeCollapse(i,CLOCATION);
					activeGroupPath.Clear();
				}
				else
				{
					for (index_t i = at+1; i < activeGroupPath.Count(); i++)
					{
						SafeCollapse(i,CLOCATION);
					}
					activeGroupPath.Truncate(at+1);
				}
				auto locked = To<CommandGroup>::Cast(shared_from_this());
				ASSERT__(locked);
				activeGroupPath << locked;
			}


			if (!doHide)
				Expand();

			//SignalButtonTreeChange();
		}


		//void	CommandGroup::SignalBaseChange(CommandContainer*newParent, const int2&newBase)
		//{
		//	ASSERT_IS_NULL__(parent);
		//	baseCoords = newBase;
		//	baseCoords.x += baseCoords.y/2;
		//	parent = newParent;

		//	foreach (members,m)
		//	{
		//		if (m->second->IsGroup())
		//		{
		//			To<CommandGroup>::Cast(m->second)->SignalBaseChange(this,baseCoords + int2(0, m->first.y+1));
		//		}

		//	}
		//}

		PCommandGroup	AddChild(CommandContainer&container, const String&name)
		{
			
			PCommandGroup rs (new CommandGroup(name));
			container.Add(rs );
			return rs;
		}
		PCommandGroup	AddChild(CommandContainer&container, const String&name, const String&iconName)
		{
			PCommandGroup rs (new CustomIcon<CommandGroup>(name,iconName));
			container.Add(rs );
			return rs;
		}

	}


	void	GlobalCmd::SeeThroughUp()
	{
		if (VisualTerrain::SeeThrough::GetFocusPoint().height < Grid::CellStack::HalfRange)
			VisualTerrain::SeeThrough::Update(true, VisualTerrain::SeeThrough::LocateCamera() + 1);
	}
	void	GlobalCmd::SeeThroughDown()
	{
		if (VisualTerrain::SeeThrough::GetFocusPoint().height > -Grid::CellStack::HalfRange)
			VisualTerrain::SeeThrough::Update(true, VisualTerrain::SeeThrough::LocateCamera()-1);
	}


	void	GlobalCmd::ToggleSeeThrough()
	{
		if (selectedEntity)
		{
			Grid::LCellIdI target = VisualTerrain::SeeThrough::LocateCamera();
			if (selectedEntity->location.height <= Grid::CellStack::TopHeight)
				target.height = selectedEntity->location.height;
			VisualTerrain::SeeThrough::Update(!VisualTerrain::SeeThrough::IsEnabled(), target);
//					Grid::SeeThrough::Update(!Grid::SeeThrough::IsEnabled(), selectedEntity->location);
		}
		else
		{
			VisualTerrain::SeeThrough::Update(!VisualTerrain::SeeThrough::IsEnabled(), VisualTerrain::SeeThrough::LocateCamera());
		}
	}

	bool	GlobalCmd::SeeThroughIsActive()
	{
		return VisualTerrain::SeeThrough::IsEnabled();
	}
	bool	GlobalCmd::SeeThroughIsActive2(bool)
	{
		return VisualTerrain::SeeThrough::IsEnabled();
	}



	void	GlobalCmd::ToggleObjectView()
	{
		objectView = !objectView;
	}

	bool	GlobalCmd::ObjectsAreHidden()
	{
		return !objectView;
	}



	
	void	CommonCmd::DemolishSelected()
	{
		ApplicationInterface::SignalRemoval(selectedEntity);
	}

	bool CommonCmd::IsModeActive(CursorMode mode)
	{
		//PMobileEntity v = To<MobileEntity>::DynamicCast(e);
		return /*v && */activeCursorMode == mode;
	}
		
	bool CommonCmd::IsModeActiveOrIdle(CursorMode mode)
	{
		//PMobileEntity v = To<MobileEntity>::DynamicCast(e);
		return /*v &&*/ (activeCursorMode == mode || activeCursorMode == CursorMode::Idle);
	}
	

	void CommonCmd::ToggleMode(CursorMode mode)
	{
		PMobileEntity v = selectedMobileEntity;
		PTower t = ToTower(selectedEntity);
		if (v || (mode == CursorMode::AttackMode && t))
		{
			if (activeCursorMode == mode)
				activeCursorMode = CursorMode::Idle;
			else
			{
				activeCursorMode = mode;
			}
		}
		else
			activeCursorMode = CursorMode::Idle;	//can't do mode

		UpdateChoices(Network::ControlEnabled());
	}

	void CommonCmd::ActivateMode(CursorMode mode)
	{
		if (activeCursorMode == mode)
			return;
		PMobileEntity v = selectedMobileEntity;
		PTower t = ToTower(selectedEntity);
		if (v || (mode == CursorMode::AttackMode && t))
		{
			activeCursorMode = mode;
		}
		else
			activeCursorMode = CursorMode::Idle;	//can't do mode

		UpdateChoices(Network::ControlEnabled());
	}


	void					VehicleCmd::AbortOperation()
	{
		#ifdef CLIENT
			if (CommonCmd::IsModeActive(CursorMode::BuildBlockMode) || CommonCmd::IsModeActive(CursorMode::RemoveBlockMode))
			{
				buildCandidates.Clear();
				removeCandidates.Clear();
				activeCursorMode = CursorMode::Idle;
				return;
			}
			activeCursorMode = CursorMode::Idle;


		#endif

		if (selectedMobileEntity || ToTower(selectedEntity))
		{
			ApplicationInterface::SignalDropAllTasks(selectedEntity);
			//v->ClearTasks();
		}

		/*#ifdef CLIENT
			UpdateChoices(true);
		#endif*/
	}

	void					VehicleCmd::Automate()
	{
		if (selectedMobileEntity)
		{
			ApplicationInterface::SignalAutomation(selectedMobileEntity,!selectedMobileEntity->isAutomated);
			//v->ClearTasks();
		}
	}

	bool					VehicleCmd::IsAutomated()
	{
		return selectedMobileEntity && selectedMobileEntity->isAutomated;
	}


	Objects::CommandFlags	VehicleCmd::AutomatedCheck(bool controlEnabled)
	{
		Objects::CommandFlags f = CommonCmd::CommonButtonCheck(controlEnabled);
		if (IsAutomated())
			f |= Objects::CommandFlags::IsPermaActive;
		return f;
	}

	Objects::CommandFlags	VehicleCmd::SleepCheck(bool controlEnabled)
	{
		Objects::CommandFlags f = CommonCmd::CommonButtonCheck(controlEnabled);
		if (selectedEntity && selectedEntity->IsSleeping())
			f |= Objects::CommandFlags::IsPermaActive;
		return f;
	}


	void					VehicleCmd::Sleep()
	{
		if (selectedEntity)
			ApplicationInterface::SignalSleep(selectedEntity, !selectedEntity->IsSleeping());
	}

	void					VehicleCmd::UndoOperation()
	{
		if (CommonCmd::IsModeActive(CursorMode::BuildBlockMode))
		{
			Objects::buildCandidates.EraseLast();
			return;
		}
		if (CommonCmd::IsModeActive(CursorMode::RemoveBlockMode))
		{
			Objects::removeCandidates.EraseLast();
			return;
		}

		if (selectedMobileEntity || ToTower(selectedEntity))
			ApplicationInterface::SignalDropLastTask(selectedEntity);
	//		v->DropLastTask();
		//#ifdef CLIENT
		//	UpdateChoices(true);
		//#endif
	}



}

#endif
