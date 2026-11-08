#ifdef HAS_WINDOW

#include <global_root.h>
#include "menuBackdrop.h"

#include <grid/planet.h>
#include <grid/worldData.h>
#include <grid/setup.h>
#include "visual/scene/world.h"
#include "visual/scene/terrain.h"
#include "configurationMode.h"
#include "gridControl.h"
#include "network.h"
#include "clientEvents.h"
#include "clientRuntimeConfig.h"

namespace MenuBackdrop
{
	namespace Detail
	{
		bool		showing = false;
		/**
		Set if generation failed (e.g. no presets found), so that it is not retried every frame
		*/
		bool		failed = false;
		Random		rng;


		/**
		Collects all predefined planet files, excluding the documentation template
		*/
		void		ListPresets(Vector<FileSystem::File>&outFiles)
		{
			outFiles.Clear();
			FileSystem::FolderScan folder = FileSystem::Directory().FindDirectory(L"../predefinedPlanets");
			if (!folder.IsValidLocation())
				return;
			FileSystem::File file;
			while (folder.NextFile(L".ini",file))
			{
				const String name = FileSystem::PathToString(file.GetInnerName()).CopyToUpperCase();
				if (name == "TEMPLATE")
					continue;
				outFiles << file;
			}
		}

		bool		IsIdle()
		{
			return !RuntimeConfig::aiMode
				&& !ConfigurationMode::IsActive()
				&& !Network::client.IsConnected()
				&& !Network::client.IsAttemptingToConnect();
		}

		bool		Generate()
		{
			Vector<FileSystem::File> presets;
			ListPresets(presets);
			if (presets.IsEmpty())
			{
				LogEvent(EventSignificance::Debug,"MenuBackdrop: No planet presets found in ../predefinedPlanets");
				return false;
			}

			const FileSystem::File&file = presets[rng.NextIndex(presets.Count())];
			Grid::ExtendedPlanetConfig config;
			if (!ConfigurationMode::LoadPlanetPreset(file.GetLocation(),config))
				return false;
			LogEvent(EventSignificance::Debug,"MenuBackdrop: Showing planet preset '"+FileSystem::PathToString(file.GetInnerName())+"'");

			config.SetSeed(rng.NextSeed());
			config.GenerateAtmosphereColor();

			//no EMP zones (same as the startup default; a previous server may have left some locked)
			Grid::empZonesUnlocked = 0xffff;
			Grid::empZonesLocked = 0x0;

			//terrain only: no NexusPresence::Create(), hence no Nexus faction, structures, or roads
			config.BuildWorld(World::actual);
			World::actual.FlushAndValidateSensory(1);	//everything visible

			ClientGrid::Apply(config);
			World::allowRecompilation = true;
			VisualTerrain::Invalidate();
			return true;
		}

		void		Tick()
		{
			if (ConfigurationMode::IsActive())
			{
				//configuration mode replaces the local terrain with its own planet
				showing = false;
				return;
			}
			if (showing || failed)
				return;
			if (!IsIdle() || World::actual.PaintingSensorsConcurrently())
				return;
			if (Generate())
				showing = true;
			else
				failed = true;	//don't retry every frame
		}
	}


	void	Create()
	{
		using namespace Detail;
		GlobalEvents::frameBegin << Tick;
		GlobalEvents::terrainFlush << []()
		{
			//whatever was shown is gone now. A new backdrop is generated once the client is idle again
			Detail::showing = false;
		};
	}

	bool	IsShowing()
	{
		return Detail::showing;
	}

	void	ClearIfShowing()
	{
		using namespace Detail;
		if (!showing)
			return;
		showing = false;
		GlobalEvents::FlushAll();
		ClientGrid::Apply(Grid::TSharedPlanetConfig());
		VisualTerrain::Invalidate();
	}
}

#endif
