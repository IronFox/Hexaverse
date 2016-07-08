#include <global_root.h>
#include "configurationMode.h"
#include "world.h"
#include "ui.h"
#include <objects/visualCommands.h>
#include "network.h"

namespace ConfigurationMode
{
	namespace Detail
	{
		bool isActive = false;
		Grid::ExtendedPlanetConfig	config, refConfig;
		UINT32 seed = 0;
		Random rng;
		Grid::WorldData serverWorld;
		bool created = false;
		bool clientWorldLoaded = false;
		Objects::PCommandGroup mountainGroup,extendedCoreGroup,liquidGroup;
		Buffer0<std::pair<int2,Objects::PBaseCommand> > buttons; 

		class MaterialDialog
		{
		public:
			GUIElements::PWindow	window;
			std::function<void(BYTE)>	onSelect;
			
			void				Create(bool allowUnknown, bool allowLiquids);
			void				SetCurrent(BYTE type);
		};

		MaterialDialog		fullScope, noUnknown, noLiquids;

		void Update()
		{
			Random random(seed);
			config.RandomizeSeeds(random);

			if (isActive)
			{
				//Grid::actualWorld.FlushAllData();
				if (!clientWorldLoaded)
				{
					clientWorldLoaded = true;
					serverWorld.swap(Grid::actualWorld);
				}
				Grid::actualWorld.BuildWorld(config);
				Grid::actualWorld.OverrideVisibility(1);
				Grid::SetAllFamiliesOutdated();	//force rebuild
				World::allowRecompilation = true;
			}
		}


		void				MaterialDialog::Create(bool allowUnknown, bool allowLiquids)
		{
			window.reset(new GUIElements::Window());
			//window->autoHeight = false;
			for (index_t i  = 0; i < Grid::Content::NumberOfPossibilities; i++)
			{
				if (!allowUnknown && i == Grid::Content::Unknown)
					continue;
				if (i == Grid::Content::Technite)
					continue;
				if (Grid::CellStack::Cell::IsLiquid((BYTE)i) && !allowLiquids)
					continue;

				String name = Grid::Content::Reinterpret(i).ToString();
				if (i == Grid::Content::Unknown)
					name = "Natural";
				GUIElements::PButton button(new GUIElements::Button(name));
				button->backgroundTone = Grid::CellStack::Cell::typeColor[i];
				Vec::abs(button->backgroundTone);
				float max = Vec::max(button->backgroundTone);
				if (max > 0.5f)
					button->backgroundTone /= max;
				button->SetColor(float4(button->backgroundTone,1.f));
				button->fillBackground = true;
				float b = 0.2f * (window->components.Count()/4);
				float x = 0.25f * (window->components.Count()%4);
				float x2 = 0.25f * (3-(window->components.Count()%4));
				button->SetLocation(Math::TFloatRect(float2(),float2(0.15f,0.15f)));
				button->anchorConfig.Left().Set(x);
				button->anchorConfig.Right().Set(x2);
				if (window->components.Count()>=4)
					button->anchorConfig.Top().Set(window->components[4*(window->components.Count()/4-1)], 0.01f);
				else
					button->anchorConfig.Top().Set(0.f);
				//if ((i%4)!=0)
				//{
				//	button->anchorConfig.Left().SetLastOf(window,0.01f);
				//}
				button->SetOnPress([this,i]()
				{
					Dialog::Hide(window);
					this->onSelect((BYTE)i);
				});
				window->components << button;
			}

			{
				GUIElements::PButton button(new GUIElements::Button("Abort"));
				float b = 0.2f * ((window->components.Count()/4)+1);
				button->SetLocation(Math::TFloatRect(float2(),float2(1.f,0.2f)));
//				button->anchorConfig.Top().Set(b);
				button->anchorConfig.Left().Set(0);
				button->anchorConfig.Right().Set(0);
				button->anchorConfig.Top().SetLastOf(window, 0.01f);
			
				button->SetOnPress([this]()
				{
					Dialog::Hide(window);
				});
				window->components << button;
			}
		}


		void				MaterialDialog::SetCurrent(BYTE type)
		{

		}



		/*
		struct TLayerConfig
		{
			int		dampening = 2;
			int		baseHeight = 0;	//center terrain height
			float	heightScale = 1.f;	//vertical scale of noise.
			//1.0f is pretty much the most you should set with normal noise frequency
			float	frequency = 1.f;	//noise frequency factor
		};

		*/


		void MakeIntButtons(const Objects::PCommandGroup&group, int&value, const int&defValue, int min, int max)
		{
			Objects::PBaseCommand reset(new Objects::CustomIconCommand("Reset this setting to server default","Undo",[&value,&defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			Objects::PBaseCommand decrease(new Objects::IconCommand("Decrease",[&value,min]()
			{
				if (value > min)
				{
					value--;
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand increase(new Objects::IconCommand("Increase",[&value,max]()
			{
				if (value < max)
				{
					value++;
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));

			Objects::PBaseCommand decreaseFast(new Objects::IconCommand("Decrease Fast",[&value,min,max]()
			{
				if (value > min)
				{
					value-=(max - min)/4;
					value = std::max(min,value);
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand increaseFast(new Objects::IconCommand("Increase Fast",[&value,min,max]()
			{
				if (value < max)
				{
					value+=(max - min)/4;
					value = std::min(max,value);
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));

			Objects::PBaseCommand setMin(new Objects::IconCommand("Set Min",[&value,min]()
			{
				if (value > min)
				{
					value = min;
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand setMax(new Objects::IconCommand("Set Max",[&value,max]()
			{
				if (value < max)
				{
					value = max;
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));


			group->Add(reset);
			group->Add(decrease);
			group->Add(increase);
			group->Add(decreaseFast);
			group->Add(increaseFast);
			group->Add(setMin);
			group->Add(setMax);
		}

		void MakeFloatButtons(const Objects::PCommandGroup&group, float&value, const float&defValue)
		{
			float min = value / 10.f;
			float max = value * 10.f;
			Objects::PBaseCommand reset(new Objects::CustomIconCommand("Reset this setting to server default","Undo",[&value,&defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			Objects::PBaseCommand decrease(new Objects::IconCommand("Decrease",[&value,min]()
			{
				if (value > min)
				{
					value /= 1.2f;
					value = std::max(value,min);
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand increase(new Objects::IconCommand("Increase",[&value,max]()
			{
				if (value < max)
				{
					value *= 1.2f;
					value = std::min(value,max);
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));

			group->Add(reset);
			group->Add(decrease);
			group->Add(increase);
		}

		Objects::PCommandGroup MakeLayer(const String&name, Grid::ExtendedPlanetConfig::TLayerConfig&layer,const Grid::ExtendedPlanetConfig::TLayerConfig&refLayer)
		{
			Objects::PCommandGroup group(new Objects::CommandGroup(name));
			group->baseCoords = int2(1);
			MakeIntButtons(group->AddChild("Base Height"),layer.baseHeight,refLayer.baseHeight, Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight);
			MakeIntButtons(group->AddChild("Dampening"),layer.dampening,refLayer.dampening, 0,Grid::CellStack::HalfRange);
			MakeFloatButtons(group->AddChild("Height Scale"),layer.heightScale, refLayer.heightScale);
			MakeFloatButtons(group->AddChild("Frequency"),layer.frequency, refLayer.frequency);
			return group;
		}


		void MakeMaterialChoice(const Objects::PCommandGroup&group, BYTE&value, const BYTE&defValue, MaterialDialog&dialog)
		{
			Objects::PBaseCommand reset(new Objects::CustomIconCommand("Reset this material choice to server default","Undo",[&value,defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			Objects::PBaseCommand choose(new Objects::IconCommand("Choose Material",[&value,&dialog]()
			{
				dialog.SetCurrent(value);
				dialog.onSelect = [&value](BYTE v)
				{
					value = v;
					Update();
				};
				Dialog::Show(dialog.window); 
			},
			[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
			));

			group->Add(reset);
			group->Add(choose);
		}

		void	Create()
		{
			if (created)
				return;
			created = true;
			fullScope.Create(true,true);
			noLiquids.Create(false,false);
			noUnknown.Create(false,true);

			mountainGroup = MakeLayer("Mountains",config.mountains,refConfig.mountains);
			extendedCoreGroup = MakeLayer("ExtendedCore",config.extendedCore,refConfig.extendedCore);
			MakeMaterialChoice(extendedCoreGroup->AddChild("Type"),config.extendedCoreType,refConfig.extendedCoreType, noUnknown);
			MakeMaterialChoice(mountainGroup->AddChild("Type"),config.innerType,refConfig.innerType, fullScope);
			MakeMaterialChoice(mountainGroup->AddChild("Top Type"),config.topType,config.topType, fullScope);
			MakeIntButtons(mountainGroup->AddChild("Snow Height"),config.snowHeight,refConfig.snowHeight,Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight);

			{
				liquidGroup.reset(new Objects::CommandGroup("Liquid"));
				liquidGroup->baseCoords = int2(1);
				MakeMaterialChoice(liquidGroup->AddChild("Type"),config.liquidCoverage,refConfig.liquidCoverage,noUnknown);
				MakeIntButtons(liquidGroup->AddChild("Level"),config.liquidHeight,refConfig.liquidHeight,Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight);
				MakeMaterialChoice(liquidGroup->AddChild("Outline"),config.liquidOutline,refConfig.liquidOutline,noLiquids);
			}


			buttons 
				<< std::make_pair(int2(0,0),
						Objects::PBaseCommand(new Objects::IconCommand("Commit",[]()
						{
							refConfig = config;
							Network::Channels::planetConfig.SendTo(Network::client,config);
							//Update();
						},
						[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
						))
					)
				<< std::make_pair(int2(1,0),
						Objects::PBaseCommand(new Objects::CustomIconCommand("Reset to server config","Undo",[]()
						{
							config = refConfig;
							Update();
						},
						[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
						))
					)
				<< std::make_pair(int2(2,0),
						Objects::PBaseCommand(new Objects::CustomIconCommand("Randomize seeds and rebuild using current config","Randomize",[]()
						{
							RandomizeSeed();
						},
						[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
						))
					)
				<< std::make_pair(int2(3,0),mountainGroup)
				<< std::make_pair(int2(4,0),extendedCoreGroup)
				<< std::make_pair(int2(5,0),liquidGroup)
			;
		}

		void	Destroy()
		{

		}
	}

	void	RandomizeSeed()
	{
		using namespace Detail;
		seed = rng.getSeed();
		Update();
	}

	void	Update(const Grid::ExtendedPlanetConfig&cfg)
	{
		using namespace Detail;
		config = cfg;
		refConfig = cfg;
		if (clientWorldLoaded)
			Update();
	}


	void	SetActive(bool newActive)
	{
		using namespace Detail;
		if (isActive == newActive)
			return;
		isActive = newActive;

		Create();
		UI::HexGrid::Configure(Objects::selectedEntity);

	}

	bool	IsActive()
	{
		using namespace Detail;
		return isActive;
	}

	bool	ReconfigureVisualCommands()
	{
		using namespace Detail;
		if (!isActive)
			return false;
		UI::HexGrid::ClearCommands();
		foreach (buttons,b)
			b->second->VisitMembers(UI::HexGrid::VisitAddCommand,false,b->first);
		return true;
	}


	void	RestoreServerWorld()
	{
		using namespace Detail;
		if (!clientWorldLoaded)
			return;
		clientWorldLoaded = false;
		serverWorld.swap(Grid::actualWorld);
		Grid::SetAllFamiliesOutdated(0);
	}


}
