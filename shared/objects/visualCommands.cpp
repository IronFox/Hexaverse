#ifdef HAS_WINDOW

#include <global_root.h>
#include "visualCommands.h"
#include "rendering.h"
#include "../../client/ExtRenderer/DirectX11.h"
#include <engine/display.h>
#include "vehicle.h"
#include "structure.h"
#include "visualSelection.h"
#include "visualChoices.h"
#include "satellite.h"
#include "faction.h"
#include "gridAndChoices.h"
#include "components.h"
#include <image/converter/magic.h>
#include "research.h"

extern Engine::Display<Engine::DirectX11>	display;
//extern Engine::VectorCamera<>				scene;

namespace Objects
{
	extern PWorldEntity selectedEntity;

	CommandContainer	globalCommands;
	PCommandGroup		viewCommands;

	/*static*/ Buffer0<CommandGroup*>		CommandGroup::activeGroupPath;


	/*static*/ GenericHashTable<PathString, DX11::Texture>	BaseCommand::textureMap;


	/*static*/ const int2 BaseCommand::coordOffset(1,0);

	

	float4 BackgroundColor(const PWorldEntity&e)
	{
		return float4(e->health > 0 && e->owner ? e->owner->activeEntityColor*0.4f : float3(),0.5f);

	}

	/**/			ResearchCommand::ResearchCommand(const PResearchTopic&topic) : Super(topic->name,topic->iconName),topic(topic)	{}

	///*virtual override*/ void	ResearchCommand::LoadIcon()
	//{
	//	if (topic->iconName.IsNotEmpty())
	//		Super::LoadIcon(topic->iconName);
	//}

	/*virtual override*/ void	ResearchCommand::Execute()
	{
		Super::Execute();
		if (selectedEntity && selectedEntity->owner &&  topic->CanResearch(selectedEntity->GetResearchState()))
		{
			ApplicationInterface::SignalResearch(topic);
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

	/*virtual override*/	void	ResearchCommand::Refresh(bool controlEnabled)
	{
		OwnerParameter owner = selectedEntity ? selectedEntity->owner : OwnerParameter();
		if (topic->IsGroup())
		{
			Super::Refresh(controlEnabled);



			if (!(state & Flags::IsPermaActive)
				&&
				owner && owner->researchState.GetActiveTopic()->IsChildOf(topic)
				)
				state |= Flags::IsFlashing | Flags::SoftFlashing;
			else
			{
				state -= Flags::IsFlashing;
				state -= Flags::SoftFlashing;
			}
		}
		else
		{
			state = BaseCommand::Flags::None;
			if (owner && owner->researchState.GetActiveTopic() == topic)
				state |= BaseCommand::Flags::IsPermaActive;
			Lua::Reference st = owner ? owner->researchState.researchState : Lua::Reference();
			if (!controlEnabled || !topic->CanResearch(st))
				return;
			state |= BaseCommand::Flags::IsEnabled;
		}
	}


	/*virtual override*/ void		ResearchCommand::CompileLabel(Label&outResult)
	{

		if (topic->IsGroup())
		{
			outResult << topic->name;
			return;
		}
		Lua::Reference state = selectedEntity ? selectedEntity->GetResearchState() : Lua::Reference();

		int current = 0;
		int max = topic->maxProgress;
		if (state.IsSet())
		{
			current = topic->currentProgress(state);
		}
//		const ResearchState*rs = selectedEntity && selectedEntity->owner ? &selectedEntity->owner->researchState : nullptr;

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
			PResearchTopic dep = topic->dependency.lock();
			if (dep && (state.IsEmpty() || dep->currentProgress(state)<dep->maxProgress))
				outResult << "Requires completion of "+dep->GetFullName()+" research";
			outResult.MarkBriefEnd();
			outResult << "";
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
				((Component::VehicleBase*)topic->componentReference)->DescribeType(outResult,false,selectedEntity ? selectedEntity->owner : OwnerParameter());
			}			
			elif (topic->componentReference->IsTurret())
				((Component::Turret*)topic->componentReference)->DescribeType(outResult,false, "Vehicle",selectedEntity ? selectedEntity->owner : OwnerParameter());
		}
	}


	/*static*/ void		EntityConstructionCommand::UpdateTexture(BaseCommand&target, Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor/* = float4(0)*/)
	{
		float distanceFactor = 1.f;
		//if (base)
		{
			#ifdef LOWER_RESOLUTION_PLANET
				distanceFactor *= 2.f;
			#endif
		}

		GeometryImage::BeginRendering(backgroundColor,distanceFactor);

		TMatrix4<> sys = Matrix<>::eye4;
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
			keyColor.Update(float4(0.5,0.5,1,1));
			return;
		}
		if (v->health <= 0)
			overallTint.Update(float4(0.25f));

		if (v->health <= 0)
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




	/*static*/ void		EntityCommand::UpdateTexture(BaseCommand&target, const PWorldEntity&e, bool isPrototype, const float4&tone/* = float4(0)*/)
	{
		float distanceFactor = 1.f;
		if (e->IsMobile())
		{
			#ifndef LOWER_RESOLUTION_PLANET
				distanceFactor *= 0.5f;
			#endif
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



	/*virtual override*/ void		BaseCommand::CompileLabel(Label&outResult)
	{
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
		outResult.lines.last() += TimeResourceTag() + String(timeCost);
	}


	/*virtual override*/ void		EntityConstructionCommand::CompileLabel(Label&outResult)
	{
		const MobileEntityProducer*p = selectedEntity ? selectedEntity->CastToProducer() : nullptr;
		//PWorkingStructure ws = To<WorkingStructure>::DynamicCast(e);

		PWorldEntity ref = reference.lock();
		int rawPrice = ref ? ref->GetConstructionCost() : Vehicle::GetPrice(base,turret,OwnerParameter());
		int price = (int)std::min(mineralsLeft, (unsigned)rawPrice);

		//if (ref || (base && turret))
		{
			UINT32 rounds = 0,rawRounds = 0;

			if (p)
				rawRounds = (unsigned)p->ProductionRounds(rawPrice,false);
			else
			{
				ASSERT__(selectedMobileEntity && selectedMobileEntity->turret->CanConstruct());
				rawRounds = selectedMobileEntity->GetOperationRounds(rawPrice,Objects::Component::Construct);
			}
			rounds = (int)std::min(roundsLeft, rawRounds);

			outResult << name;
			outResult << String(ResourceTag(Resource::Minerals))+ " "+String(price);
			if (price != rawPrice)
				outResult.lines.last() += "/"+String(rawPrice);
			if (rounds)
			{
				outResult.lines.last() += "  "+String(TimeResourceTag()) + String(rounds);
				if (rounds != rawRounds)
					outResult.lines.last() += "/"+String(rawRounds);
			}

			outResult.MarkBriefEnd();
			outResult << "";
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
				base->DescribeType(outResult,false,OwnerParameter());
			}			
			if (turret)
				turret->DescribeType(outResult,false, "Vehicle",selectedEntity->owner);

		}
	}






	/**/				DeployMobileCommand::DeployMobileCommand(const PMobileEntity&v,   FOnExecute f, FCheck c) : Super(v->GetNiceName(),v,f,c)	{}



	/*virtual override*/ void		DeployMobileCommand::CompileLabel(Label&outResult)
	{
		PHangar h = To<Hangar>::DynamicCast(selectedEntity);
		ASSERT__(h);
		PMobileEntity vehicle = ToMobileEntity( this->reference.lock() );
		if (vehicle)
		{
			outResult << "Deploy mobile unit: "+name;
			outResult.MarkBriefEnd();
			outResult << "";

			vehicle->DescribeType(outResult,Objects::WorldEntity::DescIncludeCurrentHealth | Objects::WorldEntity::DescIncludeMaxHealth | WorldEntity::DescIncludeUnitCount);
		}
	}


	void			BaseCommand::LoadIcon()
	{
		LoadIcon(name);
	}

	static bool NotValidFSChar(char c)
	{
		if (c >= 'a' && c <= 'z')
			return false;
		if (c >= 'A' && c <= 'Z')
			return false;
		if (c >= '0' && c <= '9')
			return false;
		return true;
	}

	void			BaseCommand::LoadIcon(const String&name)
	{
		String name2 = name;
		name2.replaceCharacters(NotValidFSChar,'-');

		PathString filename = PathString("resources/icons/"+name2+".png");
		if (!FileSystem::IsFile(filename))
			filename = PathString("resources/icons/Question-Mark.png");
		if (textureMap.query(filename,texture))
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



	
	void	GlobalCmd::SeeThroughUp()
	{
		if (Grid::SeeThrough::GetFocusPoint().height < Grid::CellStack::HalfRange)
			Grid::SeeThrough::Update(true, Grid::SeeThrough::LocateCamera() + 1);
	}
	void	GlobalCmd::SeeThroughDown()
	{
		if (Grid::SeeThrough::GetFocusPoint().height > -Grid::CellStack::HalfRange)
			Grid::SeeThrough::Update(true, Grid::SeeThrough::LocateCamera()-1);
	}


	void	GlobalCmd::ToggleSeeThrough()
	{
		if (selectedEntity)
		{
			Grid::LCellIdI target = Grid::SeeThrough::LocateCamera();
			if (selectedEntity->location.height <= Grid::CellStack::BottomHeight)
				target.height = selectedEntity->location.height;
			Grid::SeeThrough::Update(!Grid::SeeThrough::IsEnabled(), target);
//					Grid::SeeThrough::Update(!Grid::SeeThrough::IsEnabled(), selectedEntity->location);
		}
		else
		{
			Grid::SeeThrough::Update(!Grid::SeeThrough::IsEnabled(), Grid::SeeThrough::LocateCamera());
		}
	}

	bool	GlobalCmd::SeeThroughIsActive()
	{
		return Grid::SeeThrough::IsEnabled();
	}
	bool	GlobalCmd::SeeThroughIsActive2(bool)
	{
		return Grid::SeeThrough::IsEnabled();
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
			activeCursorMode = CursorMode::Idle;
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


	BaseCommand::Flags	VehicleCmd::AutomatedCheck(bool controlEnabled)
	{
		BaseCommand::Flags f = CommonCmd::CommonButtonCheck(controlEnabled);
		if (IsAutomated())
			f |= BaseCommand::Flags::IsPermaActive;
		return f;
	}

	BaseCommand::Flags	VehicleCmd::SleepCheck(bool controlEnabled)
	{
		BaseCommand::Flags f = CommonCmd::CommonButtonCheck(controlEnabled);
		if (selectedEntity && selectedEntity->IsSleeping())
			f |= BaseCommand::Flags::IsPermaActive;
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


	/*virtual override*/ void	CommandGroup::Execute()
	{
		bool doHide = state & Flags::IsPermaActive;

		//ASSERT__(activeGroupPath.IsEmpty() || activeGroupPath.Contains(this));

		index_t at = activeGroupPath.GetIndexOf(this);
		if (at != InvalidIndex)
		{
			for (index_t i = at+1; i < activeGroupPath.Count(); i++)
				activeGroupPath[i]->Collapse();
			if (doHide)
			{
				activeGroupPath[at]->Collapse();
			}
			activeGroupPath.Truncate(at+1);
			ASSERT_EQUAL__(at+1,activeGroupPath.Count());
		}
		else
		{
			at = activeGroupPath.GetIndexOf(parent);
			if (at == InvalidIndex)
			{
				for (index_t i = 0; i < activeGroupPath.Count(); i++)
					activeGroupPath[i]->Collapse();
				activeGroupPath.Clear();
			}
			else
			{
				for (index_t i = at+1; i < activeGroupPath.Count(); i++)
					activeGroupPath[i]->Collapse();
				activeGroupPath.Truncate(at+1);
			}
			activeGroupPath << this;
		}


		if (!doHide)
			Expand();

		//SignalButtonTreeChange();
	}

	PCommandGroup	CommandContainer::AddChild(const String&name)
	{
		PCommandGroup rs (new CommandGroup(name));
		Add(rs );
		return rs;
	}
	PCommandGroup	CommandContainer::AddChild(const int2&coords, bool relativeCoords, const String&name)
	{
		PCommandGroup rs (new CommandGroup(name));
		Add(coords, relativeCoords, rs );
		return rs;
	}
	PCommandGroup	CommandContainer::AddChild(const String&name, const String&iconName)
	{
		PCommandGroup rs (new CustomIcon<CommandGroup>(name,iconName));
		Add(rs );
		return rs;
	}
	PCommandGroup	CommandContainer::AddChild(const int2&coords, bool relativeCoords, const String&name, const String&iconName)
	{
		PCommandGroup rs (new CustomIcon<CommandGroup>(name,iconName));
		Add(coords, relativeCoords, rs );
		return rs;
	}

	index_t					CommandContainer::Find(const PBaseCommand&cmd) const
	{
		foreach (members,m)
			if (m->second == cmd)
				return m - members.pointer();
		return InvalidIndex;
	}


	void	CommandGroup::SignalBaseChange(CommandContainer*newParent, const int2&newBase)
	{
		ASSERT_IS_NULL__(parent);
		baseCoords = newBase;
		baseCoords.x += baseCoords.y/2;
		parent = newParent;

		foreach (members,m)
		{
			if (m->second->IsGroup())
			{
				To<CommandGroup>::Cast(m->second)->SignalBaseChange(this,baseCoords + int2(0, m->first.y+1));
			}

		}
	}

	PBaseCommand			CommandContainer::Add(const int2&coords, bool relativeCoords, const PBaseCommand&newCommand)
	{
		DBG_ASSERT__(Find(newCommand) == InvalidIndex);
		Member&m = members.Append();
		m.second = newCommand;
		m.first = relativeCoords ? coords : coords - baseCoords;
		if (newCommand->IsGroup())
		{
			To<CommandGroup>::Cast(newCommand)->SignalBaseChange(this,baseCoords + int2(0, m.first.y+1));
			//PCommandGroup group = To<CommandGroup>::Cast(newCommand);
			//group->baseCoords = baseCoords;
			//group->baseCoords.y += m.first.y+1;
			//group->baseCoords.x += group->baseCoords.y/2;
			//group->parent = this;
		}
		return newCommand;
	}

	PBaseCommand			CommandContainer::Add(const PBaseCommand&newCommand)
	{
		DBG_ASSERT__(Find(newCommand) == InvalidIndex);
		Member&m = members.Append();
		m.second = newCommand;
		m.first = int2((int)members.Count()-1,0);
		if (newCommand->IsGroup())
		{
			To<CommandGroup>::Cast(newCommand)->SignalBaseChange(this,baseCoords + int2(0, m.first.y+1));
/*
			PCommandGroup group = To<CommandGroup>::Cast(newCommand);
			group->baseCoords = baseCoords;
			group->baseCoords.y += m.first.y+1;
			group->baseCoords.x += group->baseCoords.y/2;
			group->parent = this;*/
		}
		return newCommand;
	}



}

#endif
