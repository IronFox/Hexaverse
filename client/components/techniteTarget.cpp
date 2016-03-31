#include <global_root.h>
#include "techniteTarget.h"
#include <objects/faction.h>
#include <runtimeConfig.h>
#include "network.h"

namespace TechniteTarget
{
	GenericHashTable<TUniqueID,Grid::LCellIdI,TUniqueID>	map;

	TUniqueID		selectedTechnite = TUniqueID::Empty();
	Grid::LCellIdI	selectedTarget = Grid::LCellIdI::Invalid;

	static void SetSelected(const TUniqueID&techniteID)
	{
		selectedTechnite = techniteID;
		selectedTarget = Grid::LCellIdI::Invalid;
		if (selectedTechnite != TUniqueID::Empty() && map.Query(selectedTechnite,selectedTarget))
		{
			ConstName empty[2] = {ConstName::Empty,ConstName::Empty};
			Objects::Marker::Create(techniteID,Objects::Marker::Type::Visited,selectedTarget,RuntimeConfig::roundNumber,techniteID,TUniqueID::Empty(),empty,RuntimeConfig::roundNumber);
		}
		Technites::Technite*t = Technites::GetTechnite(selectedTechnite);
		if (t)
			Grid::high.family[Grid::high.grid.GetCell(t->location.GetStackID()).family].SetOutdated(0.f);

	}

	static void UnsetSelected()
	{
		if (Objects::Marker::Find(selectedTechnite))
			Objects::Marker::Destroy(selectedTechnite,Network::ControlEnabled());
		Technites::Technite*t = Technites::GetTechnite(selectedTechnite);
		if (t)
			Grid::high.family[Grid::high.grid.GetCell(t->location.GetStackID()).family].SetOutdated(0.f);
		selectedTechnite = TUniqueID::Empty();
	}

	void	SignalSelection(const TUniqueID&techniteID)
	{
		if (techniteID == selectedTechnite)
			return;
		UnsetSelected();
		SetSelected(techniteID);
	}


	void	Set(const TUniqueID&techniteID, const Grid::LCellIdI&target)
	{
		map.Set(techniteID,target);
		if (techniteID == selectedTechnite)
		{
			UnsetSelected();
			SetSelected(techniteID);	//refresh target
		}


		UINT layer = Grid::CellStack::ToLayer(target.height);
		UINT stack = Grid::low.grid.GetCell(target.stackID).higherEquivalent;

		String msg = "{\"gotoStack\":"+String(stack)+",\"gotoLayer\":"+String(layer)+"}";


		Network::Channels::techniteMessage.SendTo(Network::client,Protocol::TechniteMessage(techniteID,msg));
	}

	void	Unset(const TUniqueID&techniteID)
	{
		map.Unset(techniteID);
		if (techniteID == selectedTechnite)
			UnsetSelected();
	}


	void	FlushAll()
	{
		SignalSelection(TUniqueID::Empty());
		map.Clear();
		selectedTarget = Grid::LCellIdI::Invalid;
	}


}



