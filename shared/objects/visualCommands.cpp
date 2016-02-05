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
#include <image/converter/magic.h>
#include "faction.h"

extern Engine::Display<Engine::DirectX11>	display;
//extern Engine::VectorCamera<>				scene;

namespace Objects
{
	extern PWorldEntity selectedEntity;

	Buffer0<GlobalCommand>globalCommands;

	/*static*/ GenericHashTable<PathString, DX11::Texture>	BaseCommand::textureMap;


	/*static*/ const int2 BaseCommand::coordOffset(1,0);



	/*virtual override*/ void		EntityCommand::Refresh(bool controlEnabled)
	{
		ASSERT__(check);
		state = check(selectedEntity,controlEnabled);
	}
	/*virtual override*/ void		GlobalCommand::Refresh(bool controlEnabled)
	{
		state = check(0,controlEnabled);
	}

	/*virtual override*/ void		EntityCommand::Execute() 
	{
		execute(selectedEntity);
	}



	float4 BackgroundColor(const PWorldEntity&e)
	{
		return float4(e->owner ? e->owner->activeEntityColor*0.4f : float3(),0.5f);

	}


	/*static*/ void		VehicleCommand::UpdateTexture(EntityCommand&target, Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor/* = float4(0)*/)
	{
		GeometryImage::BeginRendering(backgroundColor);

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
				if (v->isMine)
					keyColor.Update(float4(0,0,0,1));
				else
					keyColor.Update(float4(tone.rgb*0.5f,1));
			}
		#endif
		//else
	}

	/*static*/ void		VehicleCommand::UpdateTexture(EntityCommand&target, const PVehicle&v, bool isPrototype, const float4&tone/* = float4(0)*/)
	{
		GeometryImage::BeginRendering(isPrototype ? tone : BackgroundColor(v));

		TMatrix4<> sys = Matrix<>::eye4;
		sys.x.x = sys.y.y = sys.z.z = 2.5f;

		UpdateStateColors(v,tone,isPrototype);
			//keyColor.Update(float4(0.5f,0.5f,1,1));

			
		display.SetObjectMatrix(sys);
		if (v->base)
			Renderer::RenderComponent(sys,0,*v->base,0,0);
		if (v->turret)
			Renderer::RenderComponent(sys,v->base->height ,*v->turret,0,0);


		target.texture = GeometryImage::EndRendering();
	}

	void				VehicleCommand::UpdateTexture(const PVehicle&v, bool isPrototype, const float4&backgroundColor/* = float4(0)*/)
	{
		UpdateTexture(*this,v,isPrototype, backgroundColor);

	}


	void				VehicleCommand::UpdateTexture(Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor /*= float4(0)*/)
	{
		UpdateTexture(*this,base,turret,backgroundColor);
	
	}

	/*static*/ void			StructureCommand::UpdateTexture(EntityCommand&target, const PStructure&structure, bool isPrototype, const float4&tone /*= float4(0)*/)
	{
		GeometryImage::BeginRendering(isPrototype ? tone : BackgroundColor(structure));

		UpdateStateColors(structure,tone,isPrototype);
			
		display.SetObjectMatrix(Matrix<>::eye4);
		display.Render(*structure->localizableShape);	
		//_Render(*structure,float3(1),float3(1));

		float zOffset;
		PTurretBase b = structure->GetTurretBase(zOffset);
		if (b)
		{
			Renderer::RenderScaledComponent(Matrix<>::eye4, zOffset, 2.f, *b->turret, b->turretAngleX, b->turretAngleY);
			display.SetObjectMatrix(Matrix<>::eye4);
		}



		target.texture = GeometryImage::EndRendering();
	}


	void				StructureCommand::UpdateTexture(const PStructure&structure, bool isPrototype, const float4&backgroundColor /*= float4(0)*/)
	{
		UpdateTexture(*this,structure,isPrototype,backgroundColor);
	}


	/*static*/ void		SatelliteCommand::UpdateTexture(EntityCommand&target, const PSatellite&sat, bool isPrototype, const float4&tone/* = float4(0)*/)
	{
		GeometryImage::BeginRendering(isPrototype ? tone : BackgroundColor(sat),1.5f);

		//TMatrix4<> sys = Matrix<>::eye4;
		//sys.x.x = sys.y.y = sys.z.z = 2.5f;

		UpdateStateColors(sat,tone,isPrototype);
			//keyColor.Update(float4(0.5f,0.5f,1,1));

			
		//display.SetObjectMatrix(sys);
		Renderer::RenderSimpleIdentity(*sat);


		target.texture = GeometryImage::EndRendering();
	}

	void				SatelliteCommand::UpdateTexture(const PSatellite&v, bool isPrototype, const float4&backgroundColor/* = float4(0)*/)
	{
		UpdateTexture(*this,v,isPrototype, backgroundColor);

	}



	/*virtual override*/ void		IconCommand::CompileLabel(const PWorldEntity&e, Label&outResult)
	{
		int mineralCost, timeCost;
		getCosts(e,mineralCost,timeCost);

		if (timeCost == 0)
		{
			outResult << name;
			return;
		}

		outResult << name;
		if (mineralCost > 0)
		{
			outResult << " "+String(ResourceTag(Resource::Minerals))+ " "+String(mineralCost) + "  ";
		}
		else
			outResult<<" ";
		//outResult << "Rounds: "+String(timeCost);
		outResult.lines.last() += TimeResourceTag() + String(timeCost);
	}


	/*virtual override*/ void		VehicleConstructionCommand::CompileLabel(const PWorldEntity&e, Label&outResult)
	{
		
		PMobileEntityProducer p = To<MobileEntityProducer>::DynamicCast(e);
		//PWorkingStructure ws = To<WorkingStructure>::DynamicCast(e);
		ASSERT__(p);
		UINT32 rounds = 0,rawRounds = 0;
		int rawPrice = Vehicle::GetPrice(base,turret);
		int price = (int)std::min(mineralsLeft, (unsigned)rawPrice);
		if (base && turret)
		{
			rawRounds = (unsigned)p->ProductionRounds(rawPrice,false);
			rounds = (int)std::min(roundsLeft, rawRounds);
		}
		if (base && turret)
		{
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

		if (base && turret)
		{

			PVehicle v(new Vehicle(e->owner, e->location, base,turret));
			v->DescribeType(outResult,WorldEntity::DescIncludeMaxHealth|WorldEntity::DescIncludeUnitCount);
		}
		else
		{
			if (base)
			{
				//PVehicle v(new Vehicle(p->owner, p->location, base,turret));
				base->DescribeType(outResult,false);
			}			
			if (turret)
				turret->DescribeType(outResult,false, "Vehicle",e->owner);
		}

//			outResult << "Rounds: "+String(rounds);
	}

	/*virtual override*/ void		SatelliteConstructionCommand::CompileLabel(const PWorldEntity&e, Label&outResult)
	{
		
		PMobileEntityProducer p = To<MobileEntityProducer>::DynamicCast(e);
		ASSERT__(p);
		UINT32 rounds = 0;
		int price = sat->GetConstructionCost();

		rounds = (unsigned)p->ProductionRounds(price,false);


		outResult << name;
		outResult << String(ResourceTag(Resource::Minerals))+ " "+String(price);
		if (rounds)
			outResult.lines.last() += "  "+String(TimeResourceTag()) + String(rounds);

		sat->AppendDeploymentCount(outResult, e->owner);


		outResult.MarkBriefEnd();
		outResult << "";

		sat->DescribeType(outResult,WorldEntity::DescIncludeMaxHealth);

	}

	/*virtual override*/ void		MobileEntityConstructionAbortCommand::CompileLabel(const PWorldEntity&e, Label&outResult)
	{
		
		//PWorkingStructure ws = To<WorkingStructure>::DynamicCast(e);
		PMobileEntityProducer p = To<MobileEntityProducer>::DynamicCast(e);
		ASSERT__(p);
		UINT32 rounds = 0,rawRounds = 0;
		int rawPrice = sat ? sat->GetConstructionCost() : Vehicle::GetPrice(base,turret);
		int price = (int)std::min(mineralsLeft, (unsigned)rawPrice);

		rawRounds = (unsigned)p->ProductionRounds(rawPrice,false);
		rounds = (int)std::min(roundsLeft, rawRounds);


		if (sat || (base && turret))
		{
			outResult << "Active construction: "+name;
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

		if (sat)
		{
			sat->DescribeType(outResult,WorldEntity::DescIncludeMaxHealth|WorldEntity::DescIncludeUnitCount);
		}
		else
		{
			if (base && turret)
			{

				PVehicle v(new Vehicle(e->owner, e->location, base,turret));
				v->DescribeType(outResult,WorldEntity::DescIncludeMaxHealth|WorldEntity::DescIncludeUnitCount);
			}
			else
			{
				if (base)
				{
					//PVehicle v(new Vehicle(p->owner, p->location, base,turret));
					base->DescribeType(outResult,false);
				}			
				if (turret)
					turret->DescribeType(outResult,false,"Vehicle",e->owner);
			}
		}
//			outResult << "Rounds: "+String(rounds);
	}


	/**/				DeployVehicleCommand::DeployVehicleCommand(int2 coords, const PVehicle&v,   OnExecute f, Check c) : Super(coords,v->GetNiceName(),v->base,v->turret,f,c), vehicle(v)
	{}



	/*virtual override*/ void		DeployVehicleCommand::CompileLabel(const PWorldEntity&e, Label&outResult)
	{
		PVehicleStore p = To<VehicleStore>::DynamicCast(e);
		ASSERT__(p);
		if (vehicle)
		{
			outResult << "Deploy vehicle: "+name;
			outResult.MarkBriefEnd();
			outResult << "";

			vehicle->DescribeType(outResult,Objects::WorldEntity::DescIncludeCurrentHealth | Objects::WorldEntity::DescIncludeMaxHealth | WorldEntity::DescIncludeUnitCount);
		}
	}


	/*virtual override*/ void		StructureConstructionCommand::CompileLabel(const PWorldEntity&e, Label&outResult)
	{
		PMobileEntity v = To<MobileEntity>::DynamicCast(e);
		ASSERT__(v);
		int price = structure->GetConstructionCost();
		UINT32 roundsToJobCompletion = v->GetConstructionRounds(price);
		//mineralCostLeft = actuallyConsumesMinerals ? mineralCosts : 0;

		outResult << name;
		outResult << String(ResourceTag(Resource::Minerals))+ " "+String(price)+ "  ";
		outResult.lines.last() += TimeResourceTag() + String(roundsToJobCompletion);

		structure->AppendDeploymentCount(outResult, e->owner);

		outResult.MarkBriefEnd();
		outResult << "";


		structure->DescribeType(outResult,0);

		//outResult << "Rounds: "+String(roundsToJobCompletion);
	}


	void			BaseCommand::LoadIcon()
	{
		String name2 = name;
		name2.replace(' ','-');
		PathString filename = PathString("resources/icons/"+name2+".png");
		if (textureMap.query(filename,texture))
			return;
		try
		{
			Image image;
			Magic::LoadFromFile(image,filename);
			if (image.channels() < 4)
			{
				image.setChannels(4,BaseImage::CA_REPEAT);
//						Image image2(image.getWidth(),image.getHeight(),0);
////						image2.Fill(255,255,255);
//						image2.appendAlpha(&image);
//						image2.setChannels(4,BaseImage::CA_REPEAT);
//						image.swap(image2);
			}
			else
			{
						
				Concurrency::parallel_for(UINT32(0),image.getWidth(),[&image](UINT32 x)
				{
					for (UINT32 y = 0; y < image.getHeight(); y++)
					{
						BYTE*pixel = image.get(x,y);
						Vec::mul(Vec::ref3(pixel),float(pixel[3])/255.f);
					}
				});

			}
			//float3 fadeTo(0,0,255);
			//Concurrency::parallel_for(UINT32(0),image.getWidth(),[&image,fadeTo](UINT32 x)
			//{
			//	for (UINT32 y = 0; y < image.getHeight(); y++)
			//	{
			//		float fy = float(y) / float(image.getHeight()-1);
			//		BYTE*pixel = image.get(x,y);

			//		float3 px(Vec::ref3(pixel));
			//		Vec::interpolate(px,fadeTo,sqr(1.f - fy) * 0.5f * float(pixel[3])/255.f,px);
			//		Vec::clamp(px,0,255);
			//		Vec::copy(px,Vec::ref3(pixel));

			//		//color = lerp(color,float3(0,0,1),sqr(1.0 - input.uv.y)*0.5 * s.a);


			//	}
			//});
			image.flipVertical();
			texture.Load(image);
			textureMap.Set(filename,texture);
		}
		catch (const std::exception&e)
		{
			FATAL__(e.what());
		//	return false;

		}
		//return true;
	}



	
	void	GlobalCmd::SeeThroughUp(int)
	{
		if (Grid::SeeThrough::GetFocusPoint().height < Grid::CellStack::HalfRange)
			Grid::SeeThrough::Update(true, Grid::SeeThrough::LocateCamera() + 1);
	}
	void	GlobalCmd::SeeThroughDown(int)
	{
		if (Grid::SeeThrough::GetFocusPoint().height > -Grid::CellStack::HalfRange)
			Grid::SeeThrough::Update(true, Grid::SeeThrough::LocateCamera()-1);
	}


	void	GlobalCmd::ToggleSeeThrough(int)
	{
		if (selectedEntity)
		{
			Grid::LCellIdI target = Grid::SeeThrough::LocateCamera();
			if (!selectedEntity->IsSatellite())
				target.height = selectedEntity->location.height;
			Grid::SeeThrough::Update(!Grid::SeeThrough::IsEnabled(), target);
//					Grid::SeeThrough::Update(!Grid::SeeThrough::IsEnabled(), selectedEntity->location);
		}
		else
		{
			Grid::SeeThrough::Update(!Grid::SeeThrough::IsEnabled(), Grid::SeeThrough::LocateCamera());
		}
	}

	bool	GlobalCmd::SeeThroughIsActive(int)
	{
		return Grid::SeeThrough::IsEnabled();
	}
	bool	GlobalCmd::SeeThroughIsActive2(int, bool)
	{
		return Grid::SeeThrough::IsEnabled();
	}



	void	GlobalCmd::ToggleObjectView(int)
	{
		objectView = !objectView;
	}

	bool	GlobalCmd::ObjectsAreHidden(int)
	{
		return !objectView;
	}

	void	GlobalCmd::ToggleCompoundHightlights(int)
	{
		anyCompoundsColored = !anyCompoundsColored;
		if (anyCompoundsColored && !selectedMobileEntity)
		{
			Renderer::SelectCompoundAreas(selectedEntity ? selectedEntity->owner : Faction::mine);

		}
	}




	

	bool CommonCmd::IsModeActive(const PWorldEntity&e, CursorMode mode)
	{
		PMobileEntity v = To<MobileEntity>::DynamicCast(e);
		return v && activeCursorMode == mode;
	}
		
	bool CommonCmd::IsModeActiveOrIdle(const PWorldEntity&e, CursorMode mode)
	{
		PMobileEntity v = To<MobileEntity>::DynamicCast(e);
		return v && (activeCursorMode == mode || activeCursorMode == CursorMode::Idle);
	}
	

	void CommonCmd::ToggleMode(const PWorldEntity&e, CursorMode mode)
	{
		PMobileEntity v = To<MobileEntity>::DynamicCast(e);
		PTower t = To<Tower>::DynamicCast(e);
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


	void					VehicleCmd::AbortOperation(const PWorldEntity&e)
	{
		#ifdef CLIENT
			if (Component::Terraformer::Cmd::BuildBlockIsActive(e) || Component::Terraformer::Cmd::RemoveBlockIsActive(e))
			{
				buildCandidates.Clear();
				removeCandidates.Clear();
				activeCursorMode = CursorMode::Idle;
				return;
			}
			activeCursorMode = CursorMode::Idle;


		#endif

		if (e->IsMobile() || To<Tower>::DynamicCast(e))
		{
			SignalDropAllTasks(e);
			//v->ClearTasks();
		}

		/*#ifdef CLIENT
			UpdateChoices(true);
		#endif*/
	}

	void					VehicleCmd::Automate(const PWorldEntity&e)
	{
		PMobileEntity v= To<MobileEntity>::DynamicCast(e);
		if (v)
		{
			SignalAutomation(e,!v->isAutomated);
			//v->ClearTasks();
		}
	}

	bool					VehicleCmd::IsAutomated(const PWorldEntity&e)
	{
		PMobileEntity v= To<MobileEntity>::DynamicCast(e);
		return v && v->isAutomated;
	}


	BaseCommand::Flags	VehicleCmd::AutomatedCheck(const PWorldEntity&e,bool controlEnabled)
	{
		BaseCommand::Flags f = CommonCmd::CommonButtonCheck(e,controlEnabled);
		if (IsAutomated(e))
			f |= BaseCommand::Flags::IsPermaActive;
		return f;
	}


	void					VehicleCmd::UndoOperation(const PWorldEntity&e)
	{
		if (Component::Terraformer::Cmd::BuildBlockIsActive(e))
		{
			Objects::buildCandidates.EraseLast();
			return;
		}
		if (Component::Terraformer::Cmd::RemoveBlockIsActive(e))
		{
			Objects::removeCandidates.EraseLast();
			return;
		}

		if (e->IsMobile() || To<Tower>::DynamicCast(e))
			SignalDropLastTask(e);
	//		v->DropLastTask();
		//#ifdef CLIENT
		//	UpdateChoices(true);
		//#endif
	}





}

#endif
