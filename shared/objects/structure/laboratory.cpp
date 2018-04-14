#include <global_root.h>
#include "laboratory.h"
#include "../faction.h"

namespace Objects
{
	
	
	#ifdef HAS_WINDOW
	
		/*virtual override*/ void			Laboratory::DescribeType(Command::Base::Label&label, UINT32 descFlags)
		{
			label.BeginBrief();
			Super::DescribeType(label,  descFlags);
			label.MarkBriefEnd();
			label << "Laboratories research new components and buildings, or improve their attributes."
			" Each enabled laboratory contributes 1 research point per round to the currently selected faction research goal."
			" For each two enabled neighbor laboratories this output is further increased by one."
			" Research aimed at unlocking new components requires a number of research points to be accumulated."
			" Research aimed at improvement will continuously improve the selected property, but become increasingly less efficient the further research has progressed."
			" As a result, research facilities are a continuous construction effort and you will most likely never have enough."
			;
		}
	#endif

	/*virtual override*/ void			Laboratory::PrototypeSetup()
	{
		Super::PrototypeSetup();

	}

	/*virtual override*/ bool			Laboratory::IsBuilding( std::pair<int,int>&outProgress ) const
	{
		if (!IsEnabled() || !owner || owner->researchState.IsIdle())
			return false;
		auto topic = owner->researchState.GetActiveTopic();
		if (topic->maxProgress <= 0)
			return false;
		outProgress.first = topic->GetProgress(owner->researchState);
		outProgress.second = topic->maxProgress;
		return true;
	}



	/*virtual override*/ void				Laboratory::OnNewConnection(const PStructure&s, UINT32 acrossHighCell, float atTime)
	{
		Super::OnNewConnection(s,acrossHighCell,atTime);
		PLaboratory lab = To<Laboratory>::DynamicCast(s);
		if (lab)
			labNeighbors << lab;
		for (index_t i = 0; i < labNeighbors.Count(); i++)
			if (labNeighbors[i].expired())
			{
				labNeighbors.Erase(i--);
				LogEvent(EventClass::Preset::internalError,"Removing expired neighbor laboratory");
			}
	}

	/*virtual override*/ void				Laboratory::OnConnectionLost(const PStructure&s, UINT32 acrossHighCell, float atTime)
	{
		Super::OnConnectionLost(s,acrossHighCell,atTime);
		PLaboratory lab = To<Laboratory>::DynamicCast(s);
		for (index_t i = 0; i < labNeighbors.Count(); i++)
		{
			if (labNeighbors[i].lock() == lab)
				labNeighbors.Erase(i--);
			else
			if (labNeighbors[i].expired())
			{
				labNeighbors.Erase(i--);
				LogEvent(EventClass::Preset::internalError,"Removing expired neighbor laboratory");
			}
		}
	}

	count_t						Laboratory::GetTotalResearchOutput() const
	{
		count_t rs = 0;
		foreach (labNeighbors, n)
		{
			PLaboratory lab = n->lock();
			if (lab && lab->IsEnabled())
				rs++;
		}
		return 1 + rs/2;
	}


	#ifndef CLIENT

		/*virtual override*/ void			Laboratory::Tick(ResourceVector&vec)
		{
			Super::Tick(vec);

			owner->researchState.Contribute((int)GetTotalResearchOutput());
		}
	#endif

}
