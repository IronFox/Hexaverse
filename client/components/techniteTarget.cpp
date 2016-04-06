#include <global_root.h>
#include "techniteTarget.h"
#include <objects/faction.h>
#include <runtimeConfig.h>
#include "network.h"

namespace TechniteTarget
{
	Grid::HCellIdU		selectedTechnite = Grid::HCellIdU::Invalid;

	static void SetSelected(const Grid::HCellIdU&techniteID)
	{
		if (!Network::amTechniteFaction || !Objects::Faction::mine || !Objects::Faction::mine->IsTechniteFaction())
			return;
		selectedTechnite = techniteID;

		Technites::Technite*t = To<Technites::Faction>::Cast(Objects::Faction::mine)->GetOwnTechnite(selectedTechnite);
		if (t)
			Grid::high.family[Grid::high.grid.GetCell(t->location.GetStackID()).family].SetOutdated(0.f);
	}

	static void UnsetSelected()
	{
		if (!Network::amTechniteFaction || !Objects::Faction::mine || !Objects::Faction::mine->IsTechniteFaction())
			return;
		Technites::Technite*t = To<Technites::Faction>::Cast(Objects::Faction::mine)->GetOwnTechnite(selectedTechnite);
		if (t)
			Grid::high.family[Grid::high.grid.GetCell(t->location.GetStackID()).family].SetOutdated(0.f);
		selectedTechnite = Grid::HCellIdU::Invalid;
	}

	void	SignalSelection(const Grid::HCellIdU&techniteID)
	{
		if (techniteID == selectedTechnite)
			return;
		UnsetSelected();
		SetSelected(techniteID);
	}



	void	FlushAll()
	{
		SignalSelection(Grid::HCellIdU::Invalid);
	}


}



