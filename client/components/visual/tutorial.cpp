
#include <global_root.h>
#include "tutorial.h"
#include "dialog.h"
#include <io/xml.h>
#include <container/hashtable.h>
#include "../../version.h"
#include "../clientRuntimeConfig.h"
#include <objects/resource.h>

namespace Tutorial
{
	Timer::Time	lastDelayingEvent = timer.Now();
	bool			movementTutorialIssued = false,
					endRoundTutorialIssued = false;


	namespace Detail
	{
		class Message
		{
		public:
			String			image;
			Array<String>	lines;
			bool			shown = false;

			void			swap(Message&other)
			{
				image.swap(other.image);
				lines.swap(other.lines);
				std::swap(shown,other.shown);
			}
		};

		StringFile			memory;

		StringTable<Message,Strategy::Swap>	messages;
	}

	void	ShowMessage(const String&name, bool forceOpenNow/*=false*/)
	{
		if (RuntimeConfig::aiMode)
			return;
		Detail::Message*msg = Detail::messages.QueryPointer(name);
		if (!msg)
			return;
		if (msg->shown)
			return;
		msg->shown = true;
		Detail::memory << name << nl;
		Detail::memory.Flush();
		Dialog::EnqueueTutorial(name,msg->lines,msg->image);
		if (forceOpenNow)
			Dialog::ShowNextTutorial();
	}

	void	HideMessage(const String&name)
	{
		Dialog::RemoveTutorial(name);
	}



	void	Create()
	{
		XML::Container xtutorials;
		xtutorials.LoadFromFile("resources/tutorial/messages.xml");
		for (const auto&xmessage : xtutorials.root_node.children)
		{
			if (xmessage.name == "message")
			{
				String name;
				ASSERT__(xmessage.Query("name",name));
				Detail::Message*msg = Detail::messages.SetNew(name);
				xmessage.Query("image",msg->image);	//optional
				explode('\n',xmessage.inner_content,msg->lines);
				for (auto&line : msg->lines)
				{
					line.trimThis();
					line.replace("%PRODUCT_NAME%",PRODUCT_NAME);
					line.replace("%FUEL%",Objects::ResourceTag(Objects::Resource::Fuel));
					line.replace("%MINERALS%",Objects::ResourceTag(Objects::Resource::Minerals));
					line.replace("%URANIUM%",Objects::ResourceTag(Objects::Resource::Uranium));
					line.replace("%ENERGY%",Objects::ResourceTag(Objects::Resource::Energy));
					
				}
			}
			else
				FATAL__("Found unexpected entry in tutorial text file");
		
		}

		FileSystem::CreateDirectory("config");

//		#ifndef _DEBUG
		if (Detail::memory.Open(RuntimeConfig::ConfigFile("tutorialMemory.txt")))
		{
			String line;
			while (Detail::memory >> line)
			{
				Detail::Message*msg = Detail::messages.QueryPointer(line);
				if (msg)
					msg->shown = true;
			
			}
		
			Detail::memory.Close();
			Detail::memory.Append(RuntimeConfig::ConfigFile("tutorialMemory.txt"));
		}
		else
	//	#endif
			Detail::memory.Create(RuntimeConfig::ConfigFile("tutorialMemory.txt"));
	
	}



}


