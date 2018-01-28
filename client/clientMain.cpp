

#ifdef HAS_WINDOW
	#include "./ExtRenderer/DirectX11.h"
	#include <engine/display.h>
	#include <engine/auto_display_configure.h>
	#include <engine/aspect.h>
#endif

#include <math/vclasses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <general/thread_system.h>
#include <string/string_converter.h>
#include <general/random.h>
#include <image/converter/magic.h>
#ifdef HAS_WINDOW
	#include "./DX11/texture_font.h"
#endif
#include <io/config.h>
#include <container/string_list.h>
#include <io/file_system.h>

#include <objects.h>


#include <geometry/bvh.h>

#include "version.h"
#include <protocol.h>
#include <objects/faction.h>
#include <objects/structure.h>
#include <objects/gridAndChoices.h>
#include <objects/prototypes.h>
#include <objects/structure/mine.h>
#include <objects/structure/laboratory.h>
#include <boot.h>

#include "components/visual/scene/explosions.h"
#include "components/visual/scene/objectRendering.h"
#include "components/visual/objectSelection.h"
#include "components/visual/objectChoices.h"
#include "components/visual/scene/terrain.h"
#include "components/visual/hud/objectCommands.h"
#include "components/visual/hud/resourceGauges.h"
#include "components/visual/hud/hexGrid.h"
#include "components/visual/hud/statePanel.h"
#include "components/visual/hud/guiFactionTable.h"
#include "components/visual/scene/objectComponent.h"
#include "components/visual/scene/progressBar.h"
#include "components/visual/hud/dialog.h"
#include "components/visual/tutorial.h"
#include "components/visual/lineRenderer.h"
#include "components/visual/hud/surfaceRenderer.h"
#include "components/visual/hud/windowStack.h"
#include "components/ai.h"
#include "components/clientRuntimeConfig.h"
#include "components/visual/scene/world.h"
#include "components/visual/scene/lighting.h"
#include "components/visual/scene/atmosphere.h"
#include "components/visual/hud/guiHostList.h"
#include "components/visual/commandDefinitions.h"
#include "components/visual/hud/menu.h"
#include "components/visual/hud/uiScale.h"
#include "components/audio/music.h"
#include "components/visual/hud/eventLog.h"
#include "components/serverProcess.h"
#include "components/audio/audioInterface.h"
#include "components/configurationMode.h"
#include "components/gridControl.h"
#include "components/network.h"
#include "components/animation.h"
#include "components/cameraControl.h"
#include "components/clientEvents.h"
#include "components/loading.h"
#include "components/clientEvents.h"
#include "components/inputControl.h"
#include "components/objectNetworkHelper.h"
#include "components/playerControlBegin.h"

#include "rc/Resource.h"

/**
The following DWORD causes Nvidia Optimus to select the high performance GPU.
Don't change the name, value, or dllexport-declaration ("C", so it shows up as that name in the export and not some mangled thing)
*/
extern "C"
{
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

#ifdef HAS_WINDOW
	namespace DX11 = Engine::DX11; //!< Shortcut

	Engine::Display<Engine::DirectX11>	display;
	Engine::Textout<DX11::TextureFont>	textout;



#endif


Timer::Time applicationStart;










#ifdef HAS_WINDOW

	void	OnTutorialsChanged()
	{
		HUD::HexGrid::BottomRightPanel::Refresh();
	}

#endif




//void LogEvent(const EventClass&type,  const Objects::WorldEntity*e, const String&_event);



namespace ApplicationInterface
{
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick)
	{
		return
		#ifdef HAS_WINDOW
			ObjectHelper::DirectTo(v,targets, ObjectHelper::AssembleMotionFlags(detailedPath,leftClick),Objects::activeCursorMode);
		#else
			ObjectHelper::DirectTo(v,targets, ObjectHelper::AssembleMotionFlags(detailedPath,leftClick),Objects::CursorMode::Idle);
		#endif
	}

	void	SignalChoiceIntersection(const Objects::PWorldEntity&e,const Grid::LCellIdI&c, Objects::Vehicle::mode_t mode, const Objects::PMobileEntity&deployingWhat)
	{
		Protocol::ChoiceIntersection	ch;
		ch.id = e->id;
		ch.mode = (UINT16)mode;
		ch.target = c;
		if (deployingWhat)
			ch.deployingWhat.Set0() = deployingWhat->id;
		LogEvent(EventSignificance::Common,"Signalling choice intersection with "+ToString(ch.target));
		Network::Channels::choiceIntersection.SendTo(Network::client,ch);

		#ifdef HAS_WINDOW
		
			HUD::HexGrid::Configure(Objects::selectedEntity);
		#endif
	}


	
	void	SignalExtraGridSensorCreation(const World::Sensor&s)
	{
		#ifdef HAS_WINDOW
		if (s.type == World::Sensor::ShadedSphere || s.type == World::Sensor::SolidSphere || s.type == World::Sensor::UpperHalfSolidSphere)
		{
			VisualTerrain::visibleSubOrbitGrid.ChangeSensory(s.worldCenter,s.radius,1);
		}
		#endif
	}

	void	SignalExtraGridSensorRemoval(const World::Sensor&s)
	{
		#ifdef HAS_WINDOW
		if (s.type == World::Sensor::ShadedSphere || s.type == World::Sensor::SolidSphere || s.type == World::Sensor::UpperHalfSolidSphere)
		{
			VisualTerrain::visibleSubOrbitGrid.ChangeSensory(s.worldCenter,s.radius,-1);
		}
		#endif
	}


	void	SignalTargetLessAttack(const Objects::PWorldEntity&e)
	{
	//	Grid::LCellIdI loc = e->location;
		ObjectHelper::DirectTo(e,Grid::LCellIdI(e->location),  ObjectHelper::AssembleMotionFlags(false,true,false),Objects::CursorMode::AttackMode);
	}



	void	SignalConstructionAbort(const TUniqueID&plantID,const TUniqueID&taskId)
	{
		Protocol::ConstructionAbort		ca;
		ca.plantID = plantID;
		ca.taskId = taskId;
		Network::Channels::constructionAbort.SendTo(Network::client,ca);
	}

	void	SignalConstructionBegin(const TUniqueID&plantID,Objects::EntityType type)
	{
		Protocol::ConstructionBegin	cb;
		cb.plantID = plantID;
		cb.type = type;
		Network::Channels::constructionBegin.SendTo(Network::client,cb);
	}

	void	SignalRestoreCoreLink(const TUniqueID&mineId)
	{
		Network::Channels::restoreCoreLink.SendTo(Network::client,mineId);}

	void	SignalAutomation(const Objects::PWorldEntity&e, bool doAutomate)
	{
		Network::Channels::automate.SendTo(Network::client,Protocol::EntityStateUpdate(e->id,doAutomate));
	}

	void	SignalRemoval(const Objects::PWorldEntity&entity)
	{
		TUniqueID id = entity->id;
		#ifdef HAS_WINDOW
			HUD::Dialog::Confirm("Confirm destruction of "+entity->GetNiceName()+". This operation cannot be undone.",[id]()
			{
				Network::Channels::removeEntity.SendTo(Network::client,id);
	
			},
			[id]()->HUD::Dialog::CheckResult
			{
				if (!Network::client.IsConnected())
					return HUD::Dialog::CheckResult::Abort;
				Objects::PWorldEntity e = Objects::GetEntity(id);
				if (!e || !e->isOwnedByMe)
					return HUD::Dialog::CheckResult::Abort;
				if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
					return HUD::Dialog::CheckResult::OkayNotRightNow;
				return HUD::Dialog::CheckResult::OkayContinue;
			}
			);
		#else

			Network::Channels::removeEntity.SendTo(Network::client,entity->id);
		#endif
	}

	void	SignalConstructTowerTurret(const TUniqueID&towerID,const Objects::Component::Turret*&newTurret)
	{
		Protocol::ConstructionBegin	cb;
		cb.plantID = towerID;
		cb.type = newTurret->constName;
		Network::Channels::constructionBegin.SendTo(Network::client,cb);
	}

	bool	HasSignalledRoundEnd()
	{
		return Network::newRoundRequested;
	}

	bool	CanChangeRoundState()
	{
		return Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && !Network::amSpectator;
	}


	void	SignalRoundEnd()
	{
		if (Network::ControlEnabled())
		{
			Network::client.SendSignal(Protocol::ChannelID::RequestRoundEnd);
			Network::newRoundRequested = true;
			#ifdef HAS_WINDOW
				Objects::UpdateChoices(Network::ControlEnabled());
			#endif
			LogEvent(EventClass::Preset::common,nullptr,"Requested new round");
		}
	}

	void	SignalUndoRoundEnd()
	{
		if (Network::client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Network::newRoundRequested)
		{
			Network::client.SendSignal(Protocol::ChannelID::RequestContinueRound);
			Network::newRoundRequested = false;
			LogEvent(EventClass::Preset::uncommon,nullptr,"Requested to continue new round");
		}

	}
}

#ifdef HAS_WINDOW



	bool	IsFullScreen()
	{
		Resolution screen = display.GetScreenSize();
		return display.GetClientSize() == screen;
	}


	void	SetFullScreen(bool isFull)
	{
		static RECT stored;
		static bool hasAnyStored = false;
		static bool storedIsMaximized = false;
		if (isFull == IsFullScreen())
			return;
		Resolution screen = display.GetScreenSize();
		if (!isFull)
		{
			//is fullscreen => restore

			//display.ResizeWindow(screen.width,screen.height);...why was this here?
			if (hasAnyStored)
				display.PositionWindow(stored);
			display.SetBorderStyle(Engine::DisplayConfig::ResizableBorder);
			if (storedIsMaximized)
				display.Maximize();
			#ifndef _DEBUG
			::SetWindowPos(Engine::context.getWindow(),       // handle to window
				HWND_NOTOPMOST,  // placement-order handle
				0,     // horizontal position
				0,      // vertical position
				0,  // width
				0, // height
				SWP_NOMOVE | SWP_NOSIZE // window-positioning options);
				);
			#endif

		}
		else
		{
			//is not fullscreen => enter fullscreen
			if (!display.IsMaximized())
			{
				stored = display.GetWindowLocation();
				hasAnyStored = true;
				storedIsMaximized = false;
			}
			else
			{
				stored = display.GetPreMaxiMinimizeLocation();
				hasAnyStored = true;
				storedIsMaximized = true;//leave stored as is
				display.Restore();
			}
			//display.PositionWindow(0,0,screen.width,screen.height);
			display.SetBorderStyle(Engine::DisplayConfig::NoBorder);
			display.PositionWindow(0,0,screen.width,screen.height);


			#ifndef _DEBUG
			::SetWindowPos(Engine::context.getWindow(),       // handle to window
				HWND_TOPMOST,  // placement-order handle
				0,     // horizontal position
				0,      // vertical position
				0,  // width
				0, // height
				SWP_NOMOVE | SWP_NOSIZE // window-positioning options);
				);
			#endif

		}

		GlobalEvents::onFullscreenChange(isFull);
	}

#endif



void	SignalRoundEndOrMaximize()
{
	#ifdef HAS_WINDOW
		if (Engine::input.pressed[Key::Alt])
		{
			SetFullScreen(!IsFullScreen());
			return;
		}

		if (HUD::Dialog::ConfirmCurrent(false))
			return;
		if (Objects::Commands::TerraformerCommitOperation())
			return;
	#endif
	ApplicationInterface::SignalRoundEnd();

}


namespace ApplicationInterface
{




	void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, Objects::EntityType parameterName)
	{
		ObjectHelper::SignalNewJob(v,jobName,parameterName,Grid::LCellIdI::Invalid);
	}


	void	SignalTerrainModification(const Objects::PVehicle&v,Objects::Vehicle::mode_t activeCursorMode,const Array<Grid::HCellIdI>&selection)
	{
		Protocol::TerrainMod	job;
		job.id = v->id;
		job.mode = (UINT16)activeCursorMode;
		job.cells = selection;
		Network::Channels::signalTerrainMod.SendTo(Network::client,job);
	}

	void	SignalSleep(const Objects::PWorldEntity&e, bool doSleep)
	{
		Protocol::EntityStateUpdate update;
		update.entityID = e->id;
		update.doSet = doSleep;
		Network::Channels::sleep.SendTo(Network::client,update);
	}


	void	SignalDropLastTask(const Objects::PWorldEntity&e)
	{
		ObjectHelper::InnerUndoTask(e,false);
	}

	void	SignalDropAllTasks(const Objects::PWorldEntity&e)
	{
		#ifdef HAS_WINDOW
			if (e->IsMobile())
			{
				Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(e);
				if (v->constructionActionCounter > 0 
				&& v->myTasks.IsNotEmpty() 
				&& v->myTasks.Peek().IsJob() 
				&& v->myTasks.Peek().job->resourceCostLeft > 0)
				{
					TUniqueID id = e->id;
					HUD::Dialog::Confirm("Are you sure you wish to clear all tasks? The resources invested in the leading production job will be lost.",[id]()
					{
						Network::Channels::dropTasks.SendTo(Network::client, Protocol::DropTasks( id, Protocol::DropMode::AllTasks));
					},
					[id]()->HUD::Dialog::CheckResult
					{
						if (!Network::client.IsConnected() || !Objects::GetEntity(id))
							return HUD::Dialog::CheckResult::Abort;
						if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase || Network::newRoundRequested)
							return HUD::Dialog::CheckResult::OkayNotRightNow;
						return HUD::Dialog::CheckResult::OkayContinue;
					});
					return;
				}
				else
				{
			
				}
		
			}
		#endif
		Network::Channels::dropTasks.SendTo(Network::client, Protocol::DropTasks( e->id, Protocol::DropMode::AllTasks));
	}

	void	SignalDropSafeTasks(const Objects::PWorldEntity&e)
	{
		Network::Channels::dropTasks.SendTo(Network::client, Protocol::DropTasks( e->id, Protocol::DropMode::AllSafeTasks));
	}



	void SignalEnableStructure(Objects::Structure&s, bool enabled)
	{
		Protocol::EnableStructure es;
		es.id = s.id;
		es.enable = enabled;
		Network::Channels::enableStructure.SendTo(Network::client,es);
	}


}





//float							eyeHeight = 1.7f;
		



								














#ifdef HAS_WINDOW

	bool IsValidFileChar(char c)
	{
		if (CharFunctions::isalnum(c))
			return true;
		switch (c)
		{
		case ' ':
		case '-':
			return true;
		}
		return false;
	}

	PathString MakeValidFilename(String base)
	{
		base.EraseCharacters(IsValidFileChar,false);
		return PathString(base);
	}

	void WriteLabel(StringFile&file, const Objects::Description&label, const String&myTypeName)
	{


		foreach(label, line)
		{
			if (line->first.GetLength() == 0)
				continue;
			if (line->second)
				file << "*";
			String linkedLine = line->first;
			linkedLine.FindAndReplace("Hrz. Attack range","[[concepts/attack range|Hrz. Attack range]]");
			linkedLine.FindAndReplace("Base horizontal attack range","[[concepts/attack range|Base horizontal attack range]]");
			linkedLine.FindAndReplace("Initiative","[[concepts/initiative|Initiative]]");
			linkedLine.FindAndReplace("Weight","[[concepts/vehicle|Weight]]");
			linkedLine.FindAndReplace("Carrying capacity","[[concepts/vehicle|Carrying capacity]]");
//			linkedLine.FindAndReplace("Base scanning range","[[concepts/sensory|Base scanning range]]");
			linkedLine.FindAndReplace("scanning range","[[concepts/scanning range]]");
			linkedLine.FindAndReplace("Scanning range","[[concepts/scanning range|Scanning range]]");
			linkedLine.FindAndReplace("compound","[[concepts/compound]]");
			linkedLine.FindAndReplace("phase","[[concepts/phase]]");
			linkedLine.FindAndReplace("attack action","[[concepts/action pool|attack action]]");
			linkedLine.FindAndReplace("resources","[[concepts/resources]]");
			

			
			
			foreach (Objects::Prototypes::structures,s)
			{
				String name = (*s)->GetTypeName().CopyToLowerCase();
				if (name.EqualsIgnoreCase(myTypeName))
					continue;

				linkedLine.FindAndReplace(name,"[[units/structures/"+name+"]]");
			}



			foreach(linkedLine, c)
			{
				if (Objects::IsTag(*c))
				{
					file << "%"<<Objects::GetTagName(*c)<<"%";
					continue;
				}
				file << *c;
			}
			file << nl;
		}
	}

	void WriteDocumentation(const FileSystem::Folder&rootFolder, const FileSystem::Folder&outFolder, StringFile&listFile, const String&typeName, const Objects::Command::Base::Label&label, const String&pretext = "")
	{
		LogEvent(EventSignificance::Uncommon, "Documenting "+typeName);

		PathString name = MakeValidFilename( typeName );
		PathString outName = (name+".prefix").ConvertToLowerCase();
		StringFile file;
		if (!outFolder.IsValidLocation())
			FileSystem::CreateDirectory(outFolder.GetLocation());

		String relative = String(FileSystem::GetRelativePath(rootFolder.GetLocation(),outFolder.GetLocation()));
		relative.FindAndReplace('\\','/');
		PathString outPath = outFolder.FindFile(outName,false)->GetLocation();
		listFile << "* [[units/"<<relative<<'/'<<typeName<<'|'<<typeName <<"]]"<< nl;
		file.Create(outPath);

		file << "{{ img:units/"<< String(name) <<".png|"<<typeName<<"}}"<<nl;
		file << "="<<typeName<<"="<<nl;
		file << pretext;
		WriteLabel(file,label,typeName);
	}

	void	FinishRendering(const FileSystem::Folder&folder, const DX11::Texture&texture, const String&typeName)
	{
		static Image image;
		display.Copy(texture.GetHandle2D(),image);
					
		image.ScaleHalf();
		image.ScaleHalf();
		image.TruncateToOpaque();
		PathString outName = MakeValidFilename(typeName)+".png";

		png.SaveToFile(image, folder.FindFile(outName,false)->GetLocation().c_str());
	}



	namespace TimeLapse
	{
		index_t runningNumber = 0;
		bool enabled = false;
		float		frameDelaySeconds=0.2f;
		PathString	basePath;
		PathString	customBasePath;
		bool		maxCompressionPNG=false;
	}


	class StorageThread : public Sys::ThreadObject
	{
	public:
		//Image		image;
		//Semaphore	semaphore;
		//volatile bool	active;
		Timer::Time	lastSubmission = timer.Now();
		ProductionPipe<std::pair<Image,bool> >	workPipe;

		StorageThread()
		{}

		void ThreadMain() override
		{
			Array<std::shared_ptr<std::pair<Image,bool> > >	images;
			while (!application_shutting_down)
			{
				workPipe.WaitForContent();
				if (application_shutting_down)
					return;
				workPipe.SignalRead();

				const count_t numImages = workPipe.Count();
				if (numImages > images.Count())
					images.SetSize(numImages);
				for (index_t i = 0; i < numImages; i++)
					images[i].swap(workPipe[i]);
				workPipe.ExitRead();
				if (application_shutting_down)
					return;

					
				for (index_t i = 0; i < numImages; i++)
				{
					Image&image = images[i]->first;

					if (images[i]->second)
					{
						image.TruncateToOpaque();
	
						for (UINT32 x = 0; x < image.GetWidth(); x++)
							for (UINT32 y = 0; y < image.GetHeight(); y++)
							{
								BYTE*pixel = image.GetPixel(x,y);
								if (pixel[3] > 0 && pixel[3] < 255)
								{
									//#ifdef WHITE_STYLE
									//	M::TVec3<> intermediate;
									//	Vec::mult(Vec::ref3(pixel),1.f / 255.f,intermediate);
									//	float a = float(pixel[3])/255.f;
									//	Vec::addVal(intermediate,a - 1.f);
									//	Vec::div(intermediate,a);
									//	Vec::M::Clamp(intermediate,0,1);
									//	Vec::mult(intermediate,255.f,Vec::ref3(pixel));
	
									//#else
										pixel[3] = Vec::max(Vec::ref4(pixel));
										float scale = 255.f / float(pixel[3]);
										M::TVec3<> temp;
										Vec::mult(Vec::ref3(pixel),scale,temp);
										//float max = Vec::max(temp);
										Vec::clamp(temp,0,255);
										Vec::copy(temp,Vec::ref3(pixel));
			//						#endif
								}
							}
					}
					else
						image.SetChannels(3);

					if (!TimeLapse::enabled)
					{
						PathString screenshotsAt = RuntimeConfig::ConfigFile("screenshots");
						FileSystem::CreateDirectory(screenshotsAt);
						index_t counter = 0;
						PathString filename = screenshotsAt + FOLDER_SLASH_STR + PathString("screenshot"+String(counter)+".png");
						while (FileSystem::IsFile(filename))
						{
							counter++;
							filename = screenshotsAt + FOLDER_SLASH_STR + PathString("screenshot"+String(counter)+".png");
						}
						png.SaveToFile(image,filename);
						LogEvent(EventClass::Preset::uncommon, nullptr, "Screenshot written to "+String(filename));
					}
					else
					{
						String runningNumber = String(TimeLapse::runningNumber++);
						while (runningNumber.GetLength() < 4)
							runningNumber = '0'+runningNumber;
						String core = "capture"+runningNumber+".png";
						PathString filename = TimeLapse::basePath + FOLDER_SLASH + core;

						if (TimeLapse::maxCompressionPNG)
							png.SaveToFile(image,filename);
						else
							png.SaveToFileQ(image,filename);
						count_t remaining = numImages - i - 1 + workPipe.Count();
						LogEvent(EventClass::Preset::uncommon, nullptr, "Time lapse frame written to "+String(core)+". "+String(remaining)+" remaining.");
					}
				}
			}
		}

		bool	CanRun() const {return timer.GetSecondsSince(lastSubmission) > TimeLapse::frameDelaySeconds;}
		void	Run(const std::shared_ptr<std::pair<Image, bool> >&image)
		{
			if (!CanRun())
				return;

	//		active = true;
		//	semaphore.release();
			workPipe << image;
			lastSubmission = timer.Now();
		}
		void	Terminate()
		{
			ASSERT__(application_shutting_down);
			workPipe.OverrideSignalNow();
		}

	}		storageThread;




#endif













void FillMarker(Protocol::Marker&marker, const Objects::PWorldEntity&e, Protocol::MarkerReferenceEntity&outE)
{
	marker.location = e->location;
	outE.id = e->id;
	outE.owner = e->owner->id;
	outE.type = e->GetType();
}



namespace ApplicationInterface
{
	void	OnVanish(const Objects::PWorldEntity&e, Objects::RemovalType t, bool wasGhost)
	{
		#ifdef HAS_WINDOW
			if (Objects::selectedEntity == e)
			{
				Objects::Deselect(Network::ControlEnabled());
			}
		#endif
		if (wasGhost)
			return;

		#ifdef HAS_WINDOW

			Objects::PTurretBase b = e->GetTurretBase();
			if (b)
			{
				if (b->turret->continuousRotation)
					Objects::Renderer::animatedTurrets.FindAndErase(b);
				if (b->turret->CanAttack())
					Objects::Renderer::attackers.FindAndErase(e);
			}
		#endif

		if (Network::amSpectator)
			return;

		const bool allied = e->owner->IsAlliedWithMe();

		if (t == Objects::RemovalType::UncontrolledExplosion && allied)
		{
			Protocol::Marker marker;
			static_assert(Objects::Marker::Type::EntityDeath == 0, "Please update marker logic");
			marker.id.Generate();
			FillMarker(marker, e, marker.data.Set0());
			ObjectHelper::SignalMarkerCreation(marker);
			LogEvent(EventSignificance::Debug,e.get(),"Created friendly loss marker for "+e->GetName());
		}
		else
			if (t == Objects::RemovalType::Vanished && !allied)
			{
				//this event can be high-frequency, happen several times per round for the same entity,
				//and happen, in fact, so rapidly, the creation event hasn't been confirmed yet


				if (!Objects::Marker::FindByReference(e->id))//apparently, this can happen
				{
				
					Protocol::Marker marker;
					marker.id.Generate();
					static_assert(Objects::Marker::Type::HostileContact == 1, "Please update marker logic");
					FillMarker(marker, e, marker.data.Set1());
					ObjectHelper::SignalMarkerCreation(marker);

					LogEvent(EventSignificance::Debug,e.get(),"Created hostile contact marker for "+e->GetName()+" since original disappeared");
				}
			}
	}

	void	OnAppear(const Objects::PWorldEntity&e)
	{
		#ifdef HAS_WINDOW
			if (!e->ambientSound.IsCreated())
			{
				Engine::OpenAL::WaveBuffer wbo = Objects::Renderer::GetAmbientSound(*e);
				//Renderer::GetAmbientSoundOf(e->GetTypeName(),e->GetCTypeName());
				if (wbo.IsNotEmpty())
				{
					e->ambientSound.Create();
					e->ambientSound.EnqueueWaveBuffer(wbo);
					e->ambientSound.SetReferenceDistance(3.f,1.5f - (e->IsMobile()? 0.5f : 0.f));
					static Random random;
					e->ambientSound.SetPitch(1.f + random.NextFloat(-0.02f,0.02f));
					#ifdef _DEBUG
						e->ambientSound.name = e->GetName()+".ambient";
						e->ambientSound.reference = e.get();
					#endif
				}
			}

			if (e->ambientSound.IsCreated())
			{
				e->ambientSound.SetPosition(e->GetWorldPosition());
				e->ambientSound.SetVelocity(float3());
				e->ambientSound.SetGain(e->GetSoundLevel());
				e->ambientSound.SetLooping(true);
				e->ambientSound.Play();
			}


			Objects::PTurretBase b = e->GetTurretBase();
			if (b && b->turret->continuousRotation 
				#ifndef NO_HIBERNATION
					&& !e->ToHibernating()->IsHibernating()
				#endif
			)
				Objects::Renderer::animatedTurrets << b;
			if (b && b->turret->CanAttack())
				Objects::Renderer::attackers << e;
		#endif
		//fix: doesn't matter if e->isOwnedByMe is true or not. Must ALWAYS remove existing markers if we see the original again.
		//reason for this problem: entity ownership change causes vanishing, and thus ghost creation, which was not removed here, because e was suddenly mine.
		Objects::Marker*m = Objects::Marker::FindByReference(e->id);
		if (m)
		{
			LogEvent(EventSignificance::Debug,e.get(),"Deleting marker "+m->GetName()+" since original reappeared");
			ObjectHelper::SignalMarkerDeletion(m->id);
		}


		if (e->isOwnedByMe)
		{
			if (To<Objects::Structure>::DynamicCast(e))
			{
				for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
				{
					Objects::Marker&m = Objects::Marker::all[i];
					if (m.type == Objects::Marker::Type::EntityDeath && m.location == e->location)
					{
						LogEvent(EventSignificance::Debug,e.get(),"Deleting marker "+m.GetName()+" since new structure constructed in its place");
						ObjectHelper::SignalMarkerDeletion(m.id);
						i--;
					}
				}
			}
		}
		elif (Objects::Faction::mine)
		{
			bool isCommon = 
				#ifdef HAS_WINDOW
					Network::amSpectator;
				#else
					true;
				#endif
			if (e->owner->alliance != Objects::Faction::mine->alliance)
			{
				LogEvent(EventClass(isCommon ? EventSignificance::Common : EventSignificance::Important,EventFlavor::Loss),"Hostile contact: "+e->GetNiceName()+ (RuntimeConfig::autoNewRound && !PlayerControlBegin::pauseAutomaticRoundProgression ? ". Pausing automatic round progression.":"."));
				PlayerControlBegin::pauseAutomaticRoundProgression = true;
			}
			else
			{
				LogEvent(EventClass(EventSignificance::Common,EventFlavor::Gain),"Friendly contact: "+e->GetNiceName()+ (RuntimeConfig::autoNewRound && !PlayerControlBegin::pauseAutomaticRoundProgression ? ". Pausing automatic round progression.":"."));
			}
		}
	}




#ifdef HAS_WINDOW



	void	SignalUserMessage(const String&message)
	{
		HUD::Dialog::ShowMessage(message);

	}



	void	UpdateSelection(bool force)
	{
		if (!force && (RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase || !Animation::AllDone()))
			return;

		Tutorial::lastDelayingEvent = timer.Now();

	#ifndef ONE_COMPOUND_PER_FACTION
		if (Objects::selectedVehicle)
		{
			//if (Objects::selectedVehicle->tasks.IsNotEmpty() && Objects::selectedVehicle->tasks.Peek().job && Objects::selectedVehicle->tasks.Peek().job->mineralCostLeft)
			Objects::ResourceVector change;
			const Objects::ResourceVector&current = Objects::selectedVehicle->currentStorage;
			const Objects::ResourceVector max = Objects::selectedVehicle->GetMaxStorageCapacity();
			int sum = 0;
			foreach(Objects::selectedVehicle->myTasks, t)
				if (t->job)
					sum += t->job->resourceCostLeft;
			if (sum)
			{
				change.level[Objects::Resource::Minerals] -= sum;//-Objects::selectedVehicle->tasks.Peek().job->mineralCostLeft;
				HUD::ResourceGauges::Update(
				#ifndef NO_HIBERNATION
					Objects::selectedVehicle->GetUplinkTraffic(),
				#endif
					current, max, change);
			}
			else
				HUD::ResourceGauges::Update(
				#ifndef NO_HIBERNATION
					Objects::selectedVehicle->GetUplinkTraffic(),
				#endif
					current, max);
		}
		else if (Objects::selectedEntity && Objects::selectedEntity->IsMobile())
		{
			FATAL__("not implemented yet");
		}
		else if (Objects::selectedEntity && Objects::selectedEntity->IsStructure())
		{
			if (Objects::selectedEntity->IsAlive())	//ghosts have no health and no network
			{
				const Objects::Compound&n = Objects::compounds[To<Objects::Structure>::Cast(Objects::selectedEntity)->compoundID];
				Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(Objects::selectedEntity);
				if (ws)
				{
					Objects::ResourceVector change, storage, max;
					ws->GetResourceVector(change, storage, max);
					HUD::ResourceGauges::Update(
					#ifndef NO_HIBERNATION
						Objects::selectedEntity->GetUplinkTraffic(),
					#endif
						n.total.storage, n.total.storageMax, n.total.supply - n.total.demand, storage/*,change, ws->IsEnabled()*/);
				}
				else
					HUD::ResourceGauges::Update(
					#ifndef NO_HIBERNATION
						Objects::selectedEntity->GetUplinkTraffic(),
					#endif
						n.total.storage, n.total.storageMax, n.total.supply - n.total.demand);
			}
			else
				UI::HideExtraGauges();


		}
		else
			UI::HideExtraGauges();

	#else

		Objects::PFaction f;
		if (Objects::selectedEntity)
			f = Objects::selectedEntity->owner;
		else
			f = Objects::Faction::mine;
		if (f)
		{
			if (f->compound.allStructures.IsEmpty())
			{
				bool any = false;
				Objects::structures.Foreach([&any, f](const Objects::PStructure&s)
				{
					if (s->owner == f)
						any = true;
				});
				if (any)	//prevent stack overflow
					Objects::RefreshCompoundCoverage(0);
			}
		}
		if (f)
		{
			const Objects::Compound&compound = f->compound;
			const Objects::ResourceVector&current = compound.total.storage;
			const Objects::ResourceVector&max = compound.total.storageMax;
			Objects::ResourceVector change = compound.total.supply - compound.total.demand;
			Objects::ForeachMobileWorldEntity([f, &change](const Objects::PMobileEntity&v)
			{
				if (v->owner != f)
					return;
				if (v->myTasks.IsEmpty() || !v->myTasks.Peek().job)
					return;
				int opSpeed = v->GetOperationSpeed(v->myTasks.Peek().job->GetOperationAffinity());
				if (!opSpeed)
					return;
				change.level[Objects::Resource::Minerals] -= std::min(opSpeed, v->myTasks.Peek().job->resourceCostLeft);

			}, false);

		#ifndef NO_SATELLITES
			for (const auto&sat : Objects::satellites)
			{
				if (sat->owner == f && sat->RequiresFuel())
				{
					change.level[Objects::Resource::Fuel] -= sat->GetFuelConsumptionPerRound() + sat->GetExtraFuelDemand();
				}
			}
		#endif

			Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(Objects::selectedEntity);
			if (ws)
			{
				Objects::ResourceVector _change, storage, _max, zero;
				ws->GetResourceVector(_change, storage, _max);
				HUD::ResourceGauges::Update(
				#ifndef NO_HIBERNATION
					Objects::selectedEntity->GetUplinkTraffic(),
				#endif
					current, max, change, storage);
			}
			else
				HUD::ResourceGauges::Update(
				#ifndef NO_HIBERNATION
					Objects::selectedEntity ? Objects::selectedEntity->GetUplinkTraffic() : 0,
				#endif
					current, max, change);


		}
		else
			HUD::ResourceGauges::HideExtraGauges();


	#endif



		HUD::StatePanel::SignalChange();

	}

	void	HandleLocalEntityStatusChange()
	{
		UpdateSelection(false);
	}


	void	HandleLocalSelectionChange()
	{
		Objects::Command::CommandGroup::CollapseAll();
		if (Objects::selectedEntity)
		{
			Objects::PTurretBase base = Objects::selectedEntity->GetTurretBase();
			if (base)
			{
				if (base->turret->CanAttack())
					Tutorial::ShowMessage("Show Ranges");
				Tutorial::ShowMessage(base->turret->typeName);
			}
			else
				Tutorial::ShowMessage(Objects::selectedEntity->GetTypeName());
			if (Objects::selectedEntity->IsDeadOrGhost())
			{
				if (Objects::selectedEntity->isOwnedByMe)
					Tutorial::ShowMessage("Friendly Ghosts");
				else
					Tutorial::ShowMessage("Hostile Ghosts");
			}

			if (Objects::selectedMobileEntity && Objects::selectedMobileEntity->IsAboveGrid())
			{
				Camera::TargetSkyOrbit();
			}
			else
				Camera::TargetGroundOrbit();
		}
	}
	#endif
}


#ifdef HAS_WINDOW
	void RenderShadows(float shadowRadius, DX11::FBO&shadowMap, Engine::OrthographicAspect<>&shadowCamera, DX11::ObjectBuffer<M::TMatrix4<> >&shadowMatrix
	#ifdef CASCADING_SHADOW
		,index_t cascade
	#endif

	)
	{
		float3 center = Camera::scene.GetAbsoluteLocation() - Camera::scene.GetViewingDirection() * shadowRadius;
		Vec::setLen(center,Grid::ToRadius(0));

		const float3&up = Lighting::GetSunDirection();
		Mat::makeAxisSystem(center,-up,2,shadowCamera.viewInvert);
		Mat::invertSystem(shadowCamera.viewInvert,shadowCamera.view);
		shadowCamera.UpdateProjection2(-shadowRadius,-shadowRadius,shadowRadius,shadowRadius,100.f,-100.f);
		shadowCamera.depthTest = Engine::NormalDepthTest;


		//Objects::shadowCamera.UpdateInvert();


		//float3 t;
		//float4 c;
		//shadowCamera.Project(center,t);
		//Mat::Mult(Objects::shadowCamera.projection,float4(t,1),c);

		//display.SetDepthTest(false,false);	//disables depth output
		Objects::Renderer::RenderShadows(
			#ifdef CASCADING_SHADOW
				cascade
			#endif
		);
		VisualTerrain::RenderShadows();
	}

	#ifdef CASCADING_SHADOW
	void RenderShadows(float shadowRadius, index_t cascade)
	{
		RenderShadows(shadowRadius,Lighting::GPUResources::shadowMap[cascade],Lighting::GPUResources::shadowCamera[cascade],Lighting::GPUResources::shadowMatrix[cascade],cascade);

	}
	#endif
#endif





static Timer::Time	capTime = timer.Now();

bool loop()
{
	try
	{
		GlobalEvents::frameBegin();

	#ifndef HAS_WINDOW
		Sleep(50);
		AI::Process(0.5f);
	#else
		DebugCheckCamera();

		if (Animation::AllDone())
			Objects::SubOrbital::planes.Foreach([](const Objects::SubOrbital::PPlane&plane)
			{
				auto&s = *plane;
				if (s.IsGroundBoundVehicle())
					return;
				M::TVec3<> at = Grid::ToWorld(s.location);
				double offset = (((UINT32)s.id.bytes[1]<<16) | ((UINT32)s.id.bytes[2]<<8) | ((UINT32)s.id.bytes[3]));
				at.x += (sin(offset + Engine::timing.now))*0.025f;
				at.y += (sin(offset + Engine::timing.now*1.2784))*0.025f;
				at.z += (sin(offset + Engine::timing.now*1.43485))*0.025f;
				s.system.w.xyz = at;
			});

		#if 1
		{
			if (RuntimeConfig::FromFile::capFPS > 0)
			{

				Timer::Time passed = timer.now() - capTime;
				static const Timer::Time minTime = timer.toTicks(1.f / float(RuntimeConfig::FromFile:: capFPS));
				while (passed < minTime)
				{
					Sleep(0);
					passed = timer.now() - capTime;
				}
				capTime = timer.Now();
			}
		}
		#endif
		if (RuntimeConfig::aiMode && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase)
		{
			if (Engine::context.isMinimized())
				AI::Process(0.5f);
			else
				AI::Process(0.05f);
		}
	#endif







	#ifdef HAS_WINDOW

		if (!HUD::WindowStack::IsModal() && !Camera::locked)
		{
			{
				if (Engine::mouse.buttons.right_down || Engine::mouse.buttons.middle_down)
				{
					Camera::AbortTargetedTranslation();
					if (!InputControl::rightMouseDragged)
					{
						float2 delta = Engine::mouse.location.absolute - InputControl::rightMouseDragFrom;
						if (delta.GetLength() > 20.f)
						{
							InputControl::rightMouseDragged = true;
						}
					}
					if (InputControl::rightMouseDragged)
					{
						float2 delta = (Engine::mouse.location.windowRelative - Engine::mouse.previous_location.windowRelative);
						InputControl::rightMouseDragged = true;
						delta.x *= Engine::mouse.speed.x * -100.f;
						delta.y *= Engine::mouse.speed.y * -100.f;
						delta *= 0.5f;
						Camera::Translate(delta * -Camera::scene.retraction.z / Camera::GetCurrentOrbit());
					}
				}
			}

			float2 change;
			if (Engine::input.pressed[Key::A] || Engine::input.pressed[Key::Q])
			{
				change.x --;
			}
			if (Engine::input.pressed[Key::D] || Engine::input.pressed[Key::E])
			{
				change.x++;
			}
			if (Engine::input.pressed[Key::W])
			{
				change.y--;
			}
			if (Engine::input.pressed[Key::S])
			{
				change.y++;
			}
			if (!Vec::zero(change))
			{
				//Vec::setLen(change, Engine::timing.delta*100.f);
				Vec::mult(change,Engine::timing.delta*100.f);
				Camera::Rotate(change);
			}
			if (Engine::input.pressed[Key::Up])
			{
				Camera::Event::Zoom(Engine::timing.delta);
			}
			if (Engine::input.pressed[Key::Down])
			{
				Camera::Event::Zoom(-Engine::timing.delta);
			}
		}

		GlobalEvents::frameAfterCamera();	//camera is in frameBegin()


		

		
		{
			if (ShadedInstruction::chosenQuality != ShadedInstruction::Quality::Off)
			{
				#ifdef CASCADING_SHADOW
					for (index_t i = 0; i < Lighting::NumCascades; i++)
					{
						RenderShadows(-20.f + 30.f * (1 << i) ,i);
					}
				#else
					RenderShadows(30.f, Objects::shadowMap, Objects::shadowCamera, Objects::shadowMatrix);
				#endif

				Objects::Renderer::EndShadowRendering();
			}
		}

		display.Pick(Camera::scene);
		static Engine::Frustum<> frustum;
		Camera::scene.GetFrustum(frustum);


		//rendere scene here

		display.SetRasterizer(true, true);


		{
			//Atmosphere::Render(Grid::ToRadius(-Grid::CellStack::HalfRange),Grid::ToRadius(Grid::CellStack::HalfRange-1), Camera::scene.GetAbsoluteLocation());
			Atmosphere::Render(Camera::scene);

	//		const Grid::Resource&resource = Grid::Resource::all[Objects::activeResource];

			#if 0
				for (int i = 0; i < 20; i++)
					for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
					{
						Grid::TFamily::TData&dat = Grid::high.family[k].normal;
						dat.outdated = false;
						familiesRebuilt++;
						if (Grid::SeeThrough::IsEnabled())
						{
							Grid::high.BakeBlockInstruction(Grid::cellStack, k, Grid::SeeThrough::ClipHeight);
						}
						else
						{
							Grid::high.BakeBlockInstruction(Grid::cellStack, k, Grid::Clip::NoClip);
						}
					}


			#else
				World::Render(frustum);


			#endif
		//display.SetDepthTest(false,false);



			if (Objects::selectedEntity)
			{
				display.SetDepthTest(true,true);
				display.SetBlendMode(Engine::BlendMode::None);
				VisualTerrain::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(Objects::selectedEntity->location.height)+0.1f));
				VisualTerrain::RenderLowCell(Objects::selectedEntity->location.stackID);

				//Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(Objects::selectedEntity);
				if (Objects::selectedMobileEntity)
				{
					if (Objects::selectedMobileEntity->myTasks.IsNotEmpty())
					{
						foreach (Objects::selectedMobileEntity->myTasks.Peek().foci,f)
						{
							VisualTerrain::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(f->target.height)+0.1f));
							VisualTerrain::RenderLowCell(f->target.stackID);
						}
					}
				}
				display.SetDepthTest(true,true);
			}


			Objects::Renderer::RefreshFactionColors(!InputControl::saveScreenshot);
			Objects::Renderer::RenderScene();



			if (Animation::projectileActive)
			{
				display.SetDepthTest(false,false);
				display.SetRasterizer(true,false);
				display.SetBlendMode(Engine::BlendMode::FlareBlend);
				display.Render(Animation::trajectoryVisualization);
				display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			}


			display.SetDepthTest(true, true);
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			display.SetRasterizer(true,true);



			World::RenderWater(frustum);


			Objects::Renderer::RenderTranslucent(false);

			if (Objects::resourceView)
				World::RenderResources(frustum);

			#ifndef NO_SATELLITES
			if (Camera::SkyGridIsVisible())
				Grid::visibleOrbitGrid.Render(frustum);
			#endif

			
			if (Camera::SubOrbitalGridIsVisible())
			{
				if (!VisualTerrain::visibleSubOrbitGrid.valid)
					VisualTerrain::visibleSubOrbitGrid.ForceRecompileNow();
				VisualTerrain::visibleSubOrbitGrid.Render(frustum);
			}

			Objects::Renderer::RenderTranslucent(true);

			Objects::Renderer::RenderTranslucentBeams();

			World::RenderOverlay(frustum);

			World::RenderBuildBlocks();

			Explosions::Render(Camera::scene);






			Objects::Renderer::RenderProgressBars();

			if (Objects::choiceVis.showArrow && !HUD::WindowStack::IsModal())
			{
	//			display.SetDepthTest(false, false);
				bool includeWater = false;
				if (Objects::activeCursorMode == Objects::CursorMode::AttackMode)
					includeWater = true;
				else
					if (Objects::selectedVehicle /*&& Objects::selectedVehicle->base->moveOnWater*/)
						includeWater = true;
				Grid::LCellIdI loc;
				Objects::choiceVis.hasArrowCellTarget = InputControl::GetLowCellUnderCursor(loc,includeWater, Objects::activeCursorMode == Objects::CursorMode::AttackMode);
				if (Objects::choiceVis.hasArrowCellTarget && loc != Objects::choiceVis.arrowCellTarget.cellLocation)
					Objects::choiceVis.arrowCellTarget = Objects::TrajectoryEndPoint(loc);
				if (Objects::choiceVis.hasArrowCellTarget)
				{
				
					if (Objects::activeCursorMode == Objects::CursorMode::AttackMode
					 && 
						(
							(Objects::selectedMobileEntity && !Objects::selectedMobileEntity->CanHitFrom(Objects::selectedMobileEntity->GetLastPointOfOrigin(),Objects::choiceVis.arrowCellTarget))
							||
							(Objects::selectedEntity && !Objects::selectedEntity->CanHit(Objects::choiceVis.arrowCellTarget))
						)
					 //!Objects::Turret::CanHit(Objects::selectedVehicle->GetFlatAttackRange(),Objects::selectedVehicle->GetLastPointOfOrigin(),Objects::arrowCellTarget)
					 )
						Objects::choiceVis.hasArrowCellTarget = false;
				}

			}
		
		}



		display.ClearDepth();

		{
		
			float3 b, d;
			Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f, b, d);
			index_t choice = InvalidIndex;
			if (!Vec::zero(d))
			{
				Vec::normalize(d);

				float distance = std::numeric_limits<float>::max();
				choice = Objects::DetectChoiceIntersection(b, d, distance);
			}




			foreach(Objects::movementChoices, choice)
			{
				VisualTerrain::UpdatePlanetRenderingParameters(Grid::ToRadius(choice->location.height + choice->visualVerticalOffset));
				VisualTerrain::RenderTransparentLowCell(choice->location.stackID);
			}
			if (choice != InvalidIndex)
			{
				const Objects::Choice&ch = Objects::movementChoices[choice];
				VisualTerrain::UpdatePlanetRenderingParameters(Grid::ToRadiusf(float(ch.location.height + ch.visualVerticalOffset) + 0.2f));
				VisualTerrain::RenderLowCell(ch.location.stackID);
			}
		}



		LineRenderer::Flush();

		//#ifdef _DEBUG
		//if (!saveScreenshot)
		//{
		//	LineRenderer::SetColor(float4(1,0,0,1));
		//	display.SetDepthTest(false,false);
		//	LineRenderer::Rasterize(Debug::lastClickPoint - float3(1,0,0),Debug::lastClickPoint + float3(1,0,0));
		//	LineRenderer::Rasterize(Debug::lastClickPoint - float3(0,1,0),Debug::lastClickPoint + float3(0,1,0));
		//	LineRenderer::Rasterize(Debug::lastClickPoint - float3(0,0,1),Debug::lastClickPoint + float3(0,0,1));
		//	LineRenderer::Flush();
		//}
		//#endif

		display.Pick(Camera::hud);
			display.SetRasterizer(true,false);
			display.SetDepthTest(false,false);
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);

			textout.SetColor(1,1,1);



			if (HUD::showControlsAndContacts && !InputControl::saveScreenshot)
			{
				static const String line = "(C) Stefan Elsen; v" PRODUCT_VERSION "; Compiled "+String(__TIMESTAMP__);
				//textout.locate(0.f,1.f - textout.GetScaledHeight());
				textout.PushState();
				textout.Scale(0.8f,0.8f);
				textout.MoveTo(1.f - textout.GetScaledWidth(line),textout.GetScaledHeight());
				textout << line;
				textout.PopState();

			}

			if (InputControl::saveScreenshot)
			{
				if (!TimeLapse::enabled 
					|| 
					(
						//RuntimeConfig::phase != RuntimeConfig::Phase::AnimationPhase 
						Animation::AllDone() 
						&& Explosions::NoneActive()
					)
					)
				{
					InputControl::saveScreenshot = false;
				}
				//display.SetBackbufferClearColor(0,0,0,0);

				//Image image;
				if (storageThread.CanRun())
				{
					if (!TimeLapse::enabled)
						LogEvent(EventClass::Preset::uncommon,"Writing screenshot to disk...");
					//else
					//	LogEvent(EventClass::Preset::uncommon,"screenshot "+String(RuntimeConfig::phase.ToString())+", "+String(Animation::AllDone())+", "+String(Explosions::NoneActive()));
					std::shared_ptr<std::pair<Image,bool> > capture(new std::pair<Image,bool>());
					display.Capture(capture->first);
					capture->second = InputControl::saveTransparentScreenshot;

					storageThread.Run(capture);
				}

			}

	
			{
	//			CommandPanel::Print();;

				if (Objects::selectedEntity)
					HUD::StatePanel::Render(*Objects::selectedEntity);

				//if (Objects::Network::Transfer::isActive)
				//	TransferPanel::Render(Objects::Network::Transfer::fromVehicle, Objects::Network::Transfer::toNetwork);
				//else
				//	TransferPanel::Invalidate();
			}

			if (HUD::showControlsAndContacts)
			{
				
				HUD::VisualEventLog::Render();

				textout.SetColor(1,1,1);
				textout.MoveTo(0,1);
				textout << (Network::amOnline? "online" : "offline")<<", "<< (Network::haveFaction ? "logged in" : "not logged in")<< nl;
				textout << "Hello "<<Network::chosenNickname<<nl;
				if (Network::amOnline)
				{
					textout << "Server ID: " << Network::serverIDString<<nl;
					textout << "World ID: " << Network::worldIDString<<nl;
					textout << "Game Goal: "<<RuntimeConfig::gameGoal.ToString() << nl;
					#ifdef _DEBUG
						textout << "Access ID: " << Network::activeTokenString<<nl;
						textout << "Phase: "<<RuntimeConfig::phase.ToString()<<nl;
					#endif
					textout << "Round Number: "<<RuntimeConfig::roundNumber.main<<nl;
					if (!Network::gameProgress.gameOver)
					{
						if (Network::gameProgress.status.IsNotEmpty())
							textout << "Progress: "<<Network::gameProgress.status<<nl;
						if (Network::gameProgress.warning.IsNotEmpty())
						{
							textout.SetColor(1,0,0);
							textout << "Warning: "<<Network::gameProgress.warning<<nl;
							textout.SetColor(1,1,1);
						}
					}
					else
						if (Network::gameProgress.gameOver)
						{
							if (Network::gameProgress.gameWon)
							{
								textout.SetColor(0.4,0.5,1);
								textout << "YOU WIN"<<nl;
							}
							else
							{
								textout.SetColor(1,0.5,0.4);
								textout << "GAME OVER"<<nl;
							}
							textout.SetColor(1,1,1);
						}
				}
				#ifdef _DEBUG
					if (Network::haveID)
						textout << "My ID: "<<Network::myIDString<<nl;
					textout << "Active Research Script: "<< Bin2Str::ConvertPOD( Objects::ResearchState::GetScriptIdentifier() )<<nl;
					textout << -Camera::scene.retraction.z/WorldScale << nl;
				#endif
				if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Network::roundTimer > 0)
				{
					textout.PushState();
					float remaining = std::max(Network::roundTimer - timer.GetSecondsSince(Network::phaseStarted),0.f);
					textout << "Remaining seconds: "<<nl;
					textout.Scale(2.f,2.f);
					if (remaining <= 10.f && !Network::newRoundRequested)
					{
						textout.Scale(1.2f,1.2f);
						bool blinkOn = ((timer.Now()/(timer.GetTicksPerSecond()/5))%2) != 0;
						textout.SetColor(1,0.2,0.2,blinkOn ? 0.5f : 1.f);
					}
					textout<<M::Round(remaining)<<nl;
					textout.PopState();
				}


				textout.MoveTo(0.0f,0.05f);
				textout.SetColor(1,1,1);
				textout << "FPS: "<<M::RoundToInt(1.f / Engine::timing.delta) << nl;
				if (Camera::locked && !TimeLapse::enabled)
					textout << "Camera locked. Press F4 to unlock" << nl;
				#ifdef _DEBUG
					textout << "Server sec: "<<M::RoundToInt(timer.GetSecondsSince(Network::client.GetLastPackageTime()));
				#endif

			}
	


			DebugCheckVector(scene.location,-1000,1000);
		
			HUD::ResourceGauges::Render();
			HUD::HexGrid::RenderCommands(HUD::HexGrid::GetGridConfig());
			display.Pick(Camera::hud);
			HUD::WindowStack::RenderNonModal();

			HUD::HexGrid::RenderHint();
			display.Pick(Camera::hud);
			HUD::WindowStack::RenderModal();


			if (ConfigurationMode::IsActive())
			{
				static const String line = "Interactive Configuration Mode";

				textout.PushState();
				textout.Scale(2.0f,2.0f);
				textout.MoveTo(0.95f - textout.GetScaledWidth(line),0.95f);
				textout << line<<nl;
				textout.Scale(0.75f,0.75f);
				if (ConfigurationMode::ViewingServerVersion())
					textout << "Seeing Server Version";
				else
					textout << "Seeing Local Version";
				#ifdef _DEBUG
					textout << nl << ConfigurationMode::GetServerSeed();
				#endif
				textout.PopState();
			}

	#endif
		//textout << "direction: "<<scene.GetViewingDirection()<< nl;
		//textout << "position: " << scene.location << nl;
	}
	catch (const Except::Program::DataConsistencyFault&fault)
	{
		Network::SignalProtocolDisruption(fault.what());
	}

	return !application_shutting_down;// && !Engine::input.pressed[Key::Escape];
}
//void ShowFPS()
//{
//	ShowMessage(1.f/Engine::timing.delta);
//}

void MyFatal(const FatalDescriptor&descriptor)
{
	String message = descriptor.ToString();
	LogFile file(RuntimeConfig::ConfigFile("fatal.log"),false);
	
	file.Log(message+"\n",true);
	file.Close();
	LogEvent(EventClass( EventSignificance::SystemFatal, EventFlavor::SystemError ), nullptr, message);
	Logging::CallBugReporter();
	application_shutting_down = true;	//prevent certain operations in Close()
	
	//ErrMessage(message);
	//NetworkConnection::Close();
}



#ifdef HAS_WINDOW

	void	UpdateFontScale()
	{
		textout.SetScale(UI::GetScale() / display.GetClientWidth(), UI::GetScale() / display.GetClientHeight());
	}

	void	onWindowResize(const Resolution&res, UINT32 flags)
	{
		if (flags & Engine::DisplayConfig::ResizeDragHasEnded)
		{
			Camera::scene.UpdateProjection2(display.GetPixelAspect(),0.1f,1000.f);

			UpdateFontScale();

			HUD::HexGrid::OnScreenResize();
			HUD::WindowStack::SignalScreenResize();
			HUD::VisualEventLog::SignalWindowResize();
		}
	}




#endif






namespace ApplicationInterface
{
	bool	ShouldSignalRoundEnd()
	{
		if (!CanChangeRoundState())
			return false;

		if (!Objects::Faction::mine || Objects::Faction::mine->requestedNewRound || Network::amSpectator)
			return false;
		#if 0
			count_t total=0,requested=0;
			Objects::ForeachFaction([&total,&requested](const Objects::PFaction&f)
			{
				total++;
				if ((f->requestedNewRound || !f->online) && f != Objects::Faction::mine)
					requested++;
			});

			if (requested+1 != total)
				return false;
		#endif /*0*/

		Objects::PWorldEntity any;
		Objects::ForeachWorldEntity([&any](const Objects::PWorldEntity&e)
		{
			if (any || !e->isOwnedByMe)
				return;
			if (ObjectHelper::IsIdle(e))
				any = e;
		},false);
		//#ifdef _DEBUG
		//if (any)
		//	LogEvent(EventSignificance::Uncommon,any.get(),"not idle");
		//#endif
		return !any;
	}
}



void ParseKeyValue(const String&key, const String&value)
{
	try
	{
		LogEvent(EventClass::Preset::configuration, nullptr, "Setting `" + key + "` = '" + value + "'");
		if (key == "aiAccessToken")
		{
			RuntimeConfig::aiMode = true;
			RuntimeConfig::aiAccessToken.Decode(value);
			LogEvent(EventClass::Preset::configuration,nullptr,"Assuming AI-client role");
		}
		elif (key == "profile")
		{
			RuntimeConfig::profilePrefix = value+"_";
			LogEvent(EventClass::Preset::configuration,nullptr,"Using profile prefix '"+RuntimeConfig::profilePrefix+"'");
		}
		elif (key == "udpListen")
		{
			if (!convert(value.c_str(), RuntimeConfig::udpListen))
				throw Except::Program::ParameterFault(CLOCATION, "Unable to parse udpListen='" + value + "' to bool");
		}
		elif(key == "serverURL")
		{
			RuntimeConfig::serverAddress = Network::ServerAddress(value);
			LogEvent(EventClass::Preset::configuration, nullptr, "Server address set to "+ToString(RuntimeConfig::serverAddress));
		}
		elif(key == "documentHere")
		{
			FileSystem::Folder().FindDirectory(PathString(value),RuntimeConfig::documentationRootFolder,false);
			if (!RuntimeConfig::documentationRootFolder.DoesExist())
				FileSystem::CreateDirectory(RuntimeConfig::documentationRootFolder);
		}
		else
			throw Except::Program::ParameterFault(CLOCATION, "`"+key+"` is not a recognized runtime parameter");
	}
	catch (const std::exception&e)
	{
		throw Except::Program::ParameterFault(CLOCATION, "While parsing `" + key + "`='" + value + "': "+String(e.what()));
	}

}


bool ConfigureClient(const wchar_t*commandLine)
{
	Except::onFatal = MyFatal;
	
	try
	{
		String cmd = String(commandLine);
		//ShowMessage(cmd);

		StringList	tokenList;
		Tokenizer::Configuration cfg;
		cfg.convert_tabs = false;
		cfg.operators = "=";
		cfg.quotations = "\"'";
		cfg.quotation_escape_character = '\\';
		cfg.trim_characters = " \t";

		LogEvent(EventClass::Preset::configuration,"Configuring client using command line '"+cmd+"'");

		if (!Tokenizer::tokenize(cmd, cfg, tokenList))
		{
			throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Unable to parse parameter line '"+cmd+"' (missing closing quotes perhaps?)");
		}

		String parameterName;
		bool expectValue = false;
		Tokenizer::dequote(tokenList, cfg);
		foreach(tokenList, token)
		{
			if (*token == '=')
			{
				if (parameterName.IsEmpty())
					throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Missing parameter name before '='");
				expectValue = true;
				continue;
			}
			if (expectValue)
			{
				ParseKeyValue(parameterName, *token);
				expectValue = false;
				parameterName = "";
			}
			else
			{
				if (parameterName.IsNotEmpty())
					throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Found multiple tokens ('" + parameterName + "' and '" + *token + "') without intermediate '='");
				parameterName = *token;
				//expectValue = true;
			}
		}
		if (expectValue)
		{
			throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Missing parameter value for '" + parameterName + "'");
		}
		else
			if (parameterName.IsNotEmpty())
				throw Except::Program::ParameterFault(CLOCATION, "Configuration syntax violation: Missing '=' and parameter value for '" + parameterName + "'");
	

		RuntimeConfig::ClientInit();	//requires RuntimeConfig::aiMode, which is declared by parameter variables
		FileSystem::ForceCreateDirectory(RuntimeConfig::ConfigFile("logs"));	//can't do this until 
		Logging::Begin(RuntimeConfig::ConfigFile(PathString("logs/client "+Logging::TimeStamp(true)+".log")));
	}
	catch (const std::exception&e)
	{
		MyFatal(FatalDescriptor(CLOCATION, e.what()));
		return false;
	}
	if (RuntimeConfig::serverAddress.host.IsNotEmpty())
	{
		Network::ServerDiscovery::RegisterNewHost(RuntimeConfig::serverAddress, false,true);
	}

	//(PRODUCT_NAME".log",true,true)
	return true;
}

bool	IsTargetWorkingDirectory(const PathString&dir)
{
	PathString test = dir + "/resources";
	bool rs = FileSystem::IsDirectory(test);
	LogEvent(EventSignificance::Debug,"Checking client target directory candidate '"+String(dir)+"'=>'"+String(test)+"'. Result="+String(rs));


	return rs;
}

namespace StartConfig
{
	Configuration	sourceConfig,baseConfig,destConfig;
	bool			changed = false;
	PathString		configPath;

	void			Load(const PathString&userConfigPath)
	{
		configPath = userConfigPath;
		baseConfig.LoadFromFile("game.ini");
		if (!sourceConfig.LoadFromFile(userConfigPath))
		{
			LogEvent(EventClass::Preset::configuration,nullptr,"Unable to load user configuration from '"+String(userConfigPath)+"': '"+sourceConfig.Report()+"'. Attempting to load original config");
			sourceConfig = baseConfig;
		}
		destConfig = sourceConfig;
	}


	void Fix(const String&path, const String&fallback)
	{


		StringRef comment,def = fallback.ref();
		const Configuration::Attribute*attrib = baseConfig.GetAttrib(path);
		if (attrib)
		{
			comment = attrib->comment.ref();
			def = attrib->value.ref();
			LogEvent(EventClass::Preset::configurationError,"Config error: '"+path+"' not found. Defaulting to '"+(def)+"'");
		}
		else
			LogEvent(EventClass::Preset::configurationError,"Config error: '"+path+"' not found in actual or base configuration. Defaulting to '"+(def)+"'");
		destConfig.Define(path,def).comment = comment;
		changed = true;
	}

	const String&	GetString(const String&path, const String&fallback)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		if (attrib)
			return attrib->value;
		Fix(path,fallback);
		static String staticCopy;
		staticCopy = fallback;	//fallback might be a temporary string, so this at least keeps it alive
		return staticCopy;
	}

	
	bool	GetBool(const String&path, bool fallback = false)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		bool rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}
	
	int	GetInt(const String&path, int fallback = 0)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		int rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}
	
	float	GetFloat(const String&path, float fallback = 0)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		float rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}

	void	Finish()
	{
		if (changed)
		{
			LogEvent(EventClass::Preset::configuration,"Rewriting configuration ("+String(configPath)+")");
			destConfig.SaveToFile(configPath,PRODUCT_NAME " Client");
			changed = false;
		}
	}


}


int main( int argc, const char* argv[])
{
	Except::onFatal = MyFatal;
	Vec::assertConsistency<float>();
	
	#ifndef HAS_WINDOW
		String cfg;
		for (int i = 1; i < argc; i++)
			cfg += " "+String(argv[i]);
		ConfigureClient(StringW(cfg.c_str()).c_str());

	#endif



	//tests
	//{
	//	Random random;
	//	for (int i = 0; i < 1000; i++)
	//	{
	//		BaseGrid::FamiliesChanged ch;
	//		ASSERT__(ch.begin() == ch.end());
	//		count_t cnt = 0;
	//		for (int j = 0; j < 10; j++)
	//		{
	//			index_t at = random.NextIndex(BaseGrid::NumFamilies);
	//			if (!ch.changed[at])
	//				cnt++;
	//			ch.changed[at] = true;
	//		}
	//		count_t newCnt = 0;
	//		for (auto it = ch.begin(); it != ch.end(); ++it)
	//		{
	//			ASSERT__(ch.changed[*it]);
	//			newCnt++;
	//		}
	//		ASSERT_EQUAL__(cnt,newCnt);
	//	}
	//	FATAL__("Test passed. Remove test");
	//}




	try
	{
		LogEvent(EventClass::Preset::uncommon,nullptr,"Starting client v" PRODUCT_VERSION);

	//	FileSystem::CreateDirectory(RuntimeConfig::ConfigDir());
		
		{
			wchar_t path[MAX_PATH];
			ASSERT__(GetModuleFileNameW(NULL,path,MAX_PATH));
			StringW dir = FileSystem::ExtractFileDir(StringW(path));
			while (dir.GetLength() > 1 && !IsTargetWorkingDirectory(dir))
			{
				dir = FileSystem::ExtractFileDir(dir);
				LogEvent(EventSignificance::Debug,"Walking up to "+String(dir.c_str()));
			}
			if (IsTargetWorkingDirectory(dir))
				FileSystem::SetWorkingDirectory(dir);
			else
				FATAL__("Something is wrong. Unable to locate server executable, or config/declaration.xml is missing.");
		}



		{

			PathString userConfigPath = RuntimeConfig::ConfigFile("game.ini");
			if (!FileSystem::IsFile(userConfigPath))
				FileSystem::CopyFile(PathString("./game.ini"),userConfigPath,false);
			StartConfig::Load(userConfigPath);

			#ifdef HAS_WINDOW

				HUD::Menu::Configuration::Assign(StartConfig::destConfig, userConfigPath);

				String val = StartConfig::GetString("runtime/capFPS","-1");
				if (!val.EqualsIgnoreCase("off"))
					convert(val.c_str(),RuntimeConfig::FromFile::capFPS);
				RuntimeConfig::FromFile::alwaysDay = StartConfig::GetBool("runtime/alwaysDay",true);
				display.config.vertical_sync = StartConfig::GetInt("runtime/vsync");
				display.config.vertical_sync = display.config.vertical_sync > 0 ? 1 : 0;
				display.config.fsaa_samples = StartConfig::GetInt("runtime/fsaa",4);

				const String&shadowFilter = StartConfig::GetString("runtime/shadowQuality",ShadedInstruction::Quality().ToString());
				if (!ShadedInstruction::chosenQuality.Decode(shadowFilter.c_str()))
				{
					LogEvent(EventClass::Preset::configurationError,"Unable to apply chosen shadow filter '"+shadowFilter+"': Not supported");
				}

				Objects::SetShiftMeansQueue(!StartConfig::GetBool("runtime/alwaysQueueUnitCommands"));

				HUD::ResourceGauges::resourcesAsList = StartConfig::GetBool("runtime/resourcesAsList",false);

				TimeLapse::customBasePath = PathString(StartConfig::GetString("timelapse/outDirectory",""));
				TimeLapse::frameDelaySeconds = StartConfig::GetFloat("timelapse/frameDelaySeconds",0.2f);
				TimeLapse::maxCompressionPNG = StartConfig::GetBool("timelapse/maxCompressionPNG");
				
				UI::SetScale(StartConfig::GetFloat("runtime/uiScale",1.f));

				Explosions::maxConcurrentParticles = StartConfig::GetFloat("runtime/maxExplosionParticles",2000);


				if (!RuntimeConfig::aiMode && StartConfig::GetBool("runtime/soundEnabled",true))
				{
					AudioInterface::Create();
					AudioInterface::SetMasterVolume(StartConfig::GetFloat("runtime/soundVolume",1.f));
					Music::SetVolume(StartConfig::GetFloat("runtime/musicVolume",0.5f));
					Music::SetPlayMusicDuringGameplay(StartConfig::GetBool("runtime/musicDuringGameplay"));
				}

			#endif

			StartConfig::Finish();
		}

		applicationStart = timer.Now();
		Boot();
	#ifdef HAS_WINDOW
		GlobalEvents::invalidateObjectCache << ApplicationInterface::HandleLocalEntityStatusChange;
		Objects::onSelectionChanged << ApplicationInterface::HandleLocalSelectionChange;
		Objects::onSelectionChanged << [](){HUD::HexGrid::Configure(Objects::selectedEntity);};


		Engine::GlobalAspectConfiguration::worldZIsUp = true;
	#endif



		Loading::Begin();
		
		//display.create(Engine::DisplayConfig("TerraLab");

		#ifdef HAS_WINDOW
			storageThread.start();

			UINT32 loadedFlags = Engine::CreateDisplay(display,
				RuntimeConfig::ConfigFile("window"),
				RuntimeConfig::aiMode ? "AI":
				PRODUCT_NAME
				#ifdef _DEBUG
					" (DEBUG)"
				#endif
				,Resolution(1280,800),onWindowResize
				,Engine::DisplayConfig::Icon(IDI_CLIENT)
				 );
			if (loadedFlags & Engine::DisplayConfig::IsFullscreen)
			{
				SetFullScreen(true);
			}
			display.SetBackbufferClearColor(0,0,0,0);

			Loading::SetupDisplay(display);

			display.ScanShaderHeader("shaders");
			textout.GetActiveFont()->LoadFromFile("Font/font.xml");
			UpdateFontScale();
			textout.GetActiveFont()->BeginTagMode(HUD::StatePanel::OnTag,HUD::StatePanel::TagWidth,
					Objects::TextoutResourceTagOffset,(char)(Objects::TextoutResourceTagOffset+(char)Objects::Resource::NumberOfPossibilities + 2));
			GlobalEvents::uiScaleChanged += UpdateFontScale;

			Loading::SetupText(textout);


			//now we can print text

			Loading::Stage("Primitive Renderer",
				[]()
				{
					LineRenderer::Create();
					SurfaceRenderer::Create();
				});

			Loading::Stage("Control Bindings",
				[]()
				{
					Engine::input.Bind(Key::Shift, Objects::SignalShiftPressed, Objects::SignalShiftReleased);

					Engine::input.Bind(Key::Left, InputControl::SelectAndFocusPrev);
					Engine::input.Bind(Key::Right, InputControl::SelectAndFocusNext);
					Engine::input.Bind(Key::Space, InputControl::SelectAndFocusNextIdle);

					//Engine::input.Bind(Key::G, Camera::ToggleOrbit);

					Engine::input.Bind(Key::PageDown, InputControl::ViewDown);
					Engine::input.Bind(Key::PageUp, InputControl::ViewUp);

					Engine::input.Bind(Key::Alt, InputControl::ResumeBatchConstruction, InputControl::AbortBatchConstruction);

					Engine::input.Bind(Key::Backspace, InputControl::UndoTask);
					Engine::input.Bind(Key::Escape, InputControl::AbortMode);
					Engine::input.Bind(Key::Enter, SignalRoundEndOrMaximize);
					Engine::input.Bind(Key::Return, SignalRoundEndOrMaximize);
					Engine::input.Bind(Key::Tab, HUD::FactionTable::Show);
					//Engine::input.Bind(Key::T,[](){}, InputControl::QueryAndSendChatMessage);
					Engine::input.BindCtrl(Key::R,Camera::ToggleAutoRotation);
		//			Engine::input.Bind(Key::R,ToggleResourceView);
					Engine::input.Bind(Key::Ctrl,InputControl::ToggleSeeThrough);

					Engine::input.Bind(Key::H,InputControl::ToggleControlGUI);

					Engine::input.Bind(Key::M,[](){Objects::Commands::TerraformerConstruct(Objects::Prototypes::structures.Find(ConstName::Type_Mine));});
					Engine::input.Bind(Key::G,[](){Objects::Commands::TerraformerConstruct(Objects::Prototypes::structures.Find(ConstName::Type_Generator));});
					Engine::input.Bind(Key::F,[](){Objects::Commands::TerraformerConstruct(Objects::Prototypes::structures.Find(ConstName::Type_VehiclePlant));});
					Engine::input.Bind(Key::V,[](){Objects::Commands::TerraformerConstruct(Objects::Prototypes::structures.Find(ConstName::Type_VehiclePlant));});
					Engine::input.Bind(Key::R,[](){Objects::Commands::TerraformerConstruct(Objects::Prototypes::structures.Find(Objects::EntityType(ConstName::Type_Tower,ConstName::Turret_Radar)));});
					Engine::input.Bind(Key::L,[](){Objects::Commands::TerraformerConstruct(Objects::Prototypes::structures.Find(ConstName::Type_Laboratory));});


					Engine::input.BindCtrl(Key::C,ConfigurationMode::OnCopy);
					Engine::input.BindCtrl(Key::V,ConfigurationMode::OnPaste);

					Engine::input.Bind(Key::MouseButton0, InputControl::Select, HUD::WindowStack::HandleMouseUp);
					Engine::input.BindCtrl(Key::MouseButton0, InputControl::Select);
					Engine::input.Bind(Key::RightMouseButton, InputControl::BeginCameraDrag,InputControl::EndCameraDrag<true>);
					Engine::input.Bind(Key::MiddleMouseButton, InputControl::BeginCameraDrag,InputControl::EndCameraDrag<false>);

					Engine::input.Bind(Key::F4, InputControl::ToggleCameraLock);
					Engine::input.Bind(Key::F3, InputControl::SaveScreenshot);
					Engine::input.Bind(Key::F2, InputControl::ToggleTimeLapse);

					#ifdef RAYTRACE
						Engine::input.Bind(Key::Up,ZoomIn);
						Engine::input.Bind(Key::Down,ZoomOut);
					#endif

					#ifdef _DEBUG
						Engine::input.Bind(Key::X,[]()
						{
							const float baseRadius = 16;
							const float3 center = Camera::scene.GetAbsoluteLocation() - Camera::scene.GetViewingDirection() * baseRadius*2;

							Explosions::InitExplosion(center,false,baseRadius * 4, Objects::ExplosionTime(baseRadius));
						});

						Engine::input.Bind(Key::F,[]()
						{
							HUD::Dialog::Query("Enter fatal message", "", HUD::Component::Input::GetStandardAlphabet(), [](const String&input){return input.IsNotEmpty();}, [](const String&input)
							{
								if (input.IsNotEmpty())
									FATAL__(input);
							});
						});

					#endif



					Engine::mouse.BindWheel(InputControl::OnWheel);
				});

			Loading::Stage("Progress Bars",ProgressBar::Create);
			Loading::Stage("Window Stack",HUD::WindowStack::Create);
			Loading::Stage("Dialogs",HUD::Dialog::Create);
			Loading::Stage("HostList",HUD::HostList::Create);
			Loading::Stage("Voting",Vote::StaticCreate);
			Loading::Stage("VoteList",HUD::VoteList::Create);
		#endif

		Loading::Stage("Client Grid",ClientGrid::Create);

		#ifdef HAS_WINDOW
			Loading::Stage("Lighting",Lighting::Create);
			Loading::Stage("Visual Terrain",VisualTerrain::Create);		//egg-chicken
			Loading::Stage("Object Rendering", Objects::Renderer::Create);	//chicken-egg

			Loading::Stage("Configuration Mode",
				[]()
				{
					ConfigurationMode::Create();
					HUD::HexGrid::RefreshButtonState();
				});

			Loading::Stage("Object Components", Objects::CreateComponents);

			Loading::Stage("Cameras",[](){Camera::Setup(display.GetPixelAspect());});


			Loading::Stage("Explosions", Explosions::Create);
			Loading::Stage("Object Commands", Objects::Commands::DefineAll);
			Loading::Stage("Resource Gauge", HUD::ResourceGauges::Create);
			Loading::Stage("HexGrid", HUD::HexGrid::Create);
		#endif
		Loading::Stage("Animation", Animation::Create);
		Loading::Stage("Network", Network::Setup);


		#ifdef HAS_WINDOW

			Loading::Stage("Main Menu",HUD::Menu::Create);


			Loading::Stage("Atmosphere",
				[]()
				{
					if (RuntimeConfig::FromFile::alwaysDay)
					{
						M::TMatrix3<> mat,rot;
						Mat::makeAxisSystem(float3::reinterpret(Camera::scene.location).Normalized(),2,mat);
						Mat::BuildRotationMatrix(SunOffAngle, mat.x,rot);
						Mat::Mult(rot,mat.z,sunDirection);
					}


					Atmosphere::Create();
				});

			Loading::Stage("Tutorial",Tutorial::Create);
			Loading::Finish();


			{
				String name;
				try
				{
					StringFile userName(RuntimeConfig::ConfigFile(RuntimeConfig::profilePrefix+"userName.txt"));
					userName >> name;
				}
				catch (...)
				{}
				if ( Network::IsValidNickname(name))
				{
					Network::chosenNickname = name;
				}
				else
				{
				}
				HUD::Dialog::QueryNicknameIfEmpty();
			};


//			textout.SetScale(1.f/display.GetClientWidth(),1.f/display.GetClientHeight());	//apparently can't restore this...
			
			if (RuntimeConfig::documentationRootFolder.DoesExist())
			{
				{
					FileSystem::Folder imageTarget = FileSystem::Folder(RuntimeConfig::documentationRootFolder).FindDirectory("images/units",false);
					if (imageTarget.IsValidLocation())
					{
						LogEvent(EventSignificance::Uncommon, "Rendering type images to "+String(imageTarget.GetLocation())+ "...");
						Objects::GeometryImage::resultResolution = 1024;
						Objects::Command::EntityCommand container;
						Image image;
						FileSystem::Folder folder(imageTarget);
						foreach (Objects::Prototypes::all,s)
						{
							LogEvent(EventSignificance::Uncommon, "Capturing "+(*s)->GetTypeName());
							Objects::Command::EntityCommand::UpdateTexture(container,*s,true);
							FinishRendering(folder, container.texture, (*s)->GetTypeName());
						}

						foreach (Objects::Component::allTurretComponents,c)
						{
							if ((*c)->mountType == Objects::Component::Turret::MountType::Plane)
								continue;
							LogEvent(EventSignificance::Uncommon, "Capturing "+(*c)->typeName);
							Objects::Command::EntityConstructionCommand::UpdateTexture(container,nullptr,*c);
							FinishRendering(folder, container.texture, (*c)->typeName);
						}

						foreach (Objects::Component::baseComponents,c)
						{
							LogEvent(EventSignificance::Uncommon, "Capturing "+(*c)->typeName);
							Objects::Command::EntityConstructionCommand::UpdateTexture(container,*c,nullptr);
							FinishRendering(folder, container.texture, (*c)->typeName);
						}
					}
				}
				{
					FileSystem::Folder documentationTarget = FileSystem::Folder(RuntimeConfig::documentationRootFolder).FindDirectory("documentation",false);
					if (documentationTarget.IsValidLocation())
					{
						FileSystem::Folder rootFolder(documentationTarget.FindDirectory("units",false));
						FileSystem::Folder structureFolder(rootFolder.FindDirectory("structures",false));
						FileSystem::Folder planeFolder(rootFolder.FindDirectory("planes",false));
						FileSystem::Folder turretFolder(rootFolder.FindDirectory("turrets",false));
						FileSystem::Folder vbaseFolder(rootFolder.FindDirectory("vbases",false));

						StringFile unitList;
						unitList.Create(rootFolder.FindFile("list.prefix",false)->GetLocation());

						unitList << "=Unit Types and Components="<<nl;

						unitList << "==Structures=="<<nl;
						foreach (Objects::Prototypes::structures,s)
						{
							Objects::Command::Base::Label label;
							(*s)->DescribeType(label,0xFFFFFFFF & ~Objects::WorldEntity::DescIncludeName & ~Objects::WorldEntity::DescIncludeCurrentHealth);
							String pretext;
							auto tb = (*s)->GetTurretBase();
							if (tb)
							{
								label.Insert(0).first = "Based on [[units/turrets/"+String(MakeValidFilename(tb->turret->typeName))+"|"+tb->turret->typeName+"]] turret";
								label.SetBrief(0);
							}
							WriteDocumentation(rootFolder, structureFolder,unitList, (*s)->GetTypeName(), label,pretext);
						}

						unitList << "==Turrets=="<<nl;
						foreach (Objects::Component::allTurretComponents,c)
						{
							if ((*c)->mountType == Objects::Component::Turret::MountType::Plane)
								continue;
							Objects::Command::Base::Label label;
							(*c)->DescribeType(label,false,false,"",Objects::OwnerParameter());
							WriteDocumentation(rootFolder, turretFolder,unitList, (*c)->typeName, label);
						}

						unitList << "==Vehicle Bases=="<<nl;
						foreach (Objects::Component::baseComponents,c)
						{
							Objects::Command::Base::Label label;
							(*c)->DescribeType(label,false,false,Objects::OwnerParameter());
							WriteDocumentation(rootFolder, vbaseFolder,unitList, (*c)->typeName, label);
						}

						unitList << "==Sub Orbital Planes=="<<nl;
						foreach (Objects::Prototypes::planes,s)
						{
							Objects::Description label;
							(*s)->DescribeType(label,0xFFFFFFFF & ~Objects::WorldEntity::DescIncludeName & ~Objects::WorldEntity::DescIncludeCurrentHealth);
							WriteDocumentation(rootFolder, planeFolder,unitList, (*s)->GetTypeName(), label);
						}
					}
				}

			}
			else
			{

				display.Assign(loop);
				display.Execute();

			}
		#else
			Loading::Finish();
			while (!application_shutting_down && loop())
			{};
				

		#endif




		Network::Shutdown();

		#ifdef HAS_WINDOW
			ProgressBar::Destroy();
			Atmosphere::Destroy();
			Explosions::Destroy();
			LineRenderer::Destroy();
			SurfaceRenderer::Destroy();
			AudioInterface::Destroy();
			Objects::Renderer::Destroy();
			Objects::Command::VisualCommand::textureMap.Clear();
			Objects::DestroyComponents();
		#endif
		ClientGrid::Destroy();
		Animation::Destroy();


		GlobalEvents::applicationTerminate();
		application_shutting_down = true;


		#ifdef HAS_WINDOW
			storageThread.Terminate();
			display.Destroy();
		#endif
		Logging::End();

		TerminateProcess(GetCurrentProcess(),0);	//self-terminate

		return 0;
	}
	catch (const Except::Exception&exception)
	{
		FATAL__("Caught unhandled exception of type "+String(exception.GetType())+":\n"+exception.what());
	}
	catch (const std::exception&exception)
	{
		FATAL__("Caught unhandled exception of type "+String(typeid(exception).name())+":\n"+String(exception.what()));
	}
	catch (...)
	{
		FATAL__("Caught something...");
	}
	return -1;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	return main(0,NULL);
}



