--[[
This file is responsible for creating a particular distribution of resources.
If chosen, it manages the levels of all resources across the globe.

API:
void	Game.Log(msg)					Prints any message to Hexaverse log (client or server-private)

bool	Game.LowerResolutionPlanet		Set to true if the planet has reduced grid resolution (1/4 of cells, each cell twice as high)
float	Game.UnitScale					Set to a global unit scale corresponding to [Game.LowerResolutionPlanet]. This scale value is to be
										applied too units (hence the name). 1 if the planet is not of lowered resolution, 2 otherwise.

Cell[]		Game.lowCells				Ordered table of all planar cells in the world, with:
Cell
{
	Vector	center;			Normalized center vector of the cell
	Cell[]	neighbors;		Ordered table of all planar neighbor cells
	int		index;			Linear index of this cell in Game.Cells in [1,#Game.lowCells]
};
Vector
{
	float	x,y,z;
};

]]







--[[
Compute(): Expected to compute the distribution, and return a table of the following composition:

Result:
{
	Minerals = {...},
	Fuel = {...},
	Uranium = {...}
}

Each resource table must be an ordered array of non-negative integers, of length #Game.lowCells
The number corresponds the amount of resources found in the respective location.
Reference numbers are:
0 implies that the resource does not exist in that spot.
3 should be minimum where any resources are found.
12 should be the absolute maximum. For Uranium the maximum should be no more than 6
]]
function Compute()
	

end;


