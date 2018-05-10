--[[
This file is responsible for creating a particular distribution of resources.
If chosen, it manages the levels of all resources across the globe.

API:
void	Game.Log(msg)					Prints any message to Hexaverse log (client or server-private)
int		Game.RandomInt(int min, int max)		Generates a pseudo random value in the range [min,max]
float	Game.Simplex(float x, float y, float z)	Samples a raw OpenSimplex noise value for the specified coordinates. The result is in the range [-1,+1], smooth, and semi-normaly distributed

bool	Game.lowerResolutionPlanet		Set to true if the planet has reduced grid resolution (1/4 of cells, each cell twice as high)

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



function Simplex(v--[[vector]],factor)
	return Game.Simplex(v.x*factor,v.y*factor,v.z*factor);
end;

function CellSimplex(cell--[[Cell]],factor)
	return Simplex(cell.center,factor);
end;


function Sqr(x--[[float]])--float
	return x*x;
end;

function QuadraticDistance(a--[[Vector]],b--[[Vector]])--float
	return Sqr(a.x-b.x) + Sqr(a.y-b.y) + Sqr(a.z-b.z);
end;


function ResourceFactor(f--[[Cell]])--float
	local s = CellSimplex(f,10)*0.875+0.125;
	if (s < 0.25) then
		s = 0;
	end;
	return s;
end;

function PropagateResource(rs--[[resource table]], f--[[Cell]], depth--[[int]])
	local s = ResourceFactor(f);
	if (depth == 0) then
		rs.Uranium[f.index] = 6 * s;
	else if (depth == 1) then
		rs.Minerals[f.index] = s*12;
	else if (depth == 2) then
		rs.Fuel[f.index] = s*12;
		return;	--nothing left to do
	end;
	end;
	end;
	local numBranches = 2-depth;
	
	for i=1,numBranches do
		for j=1,10 do
			local n = Game.RandomInt(1,#f.neighbors);
			local at = f.neighbors[n].index;
			if (rs.Uranium[at] == 0 and rs.Minerals[at]==0 and rs.Fuel[at]==0) then
				PropagateResource(rs,f.neighbors[n],depth+1);
				break;
			end;
		end;
	end;
end;

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
	local rs = {};
	rs.Minerals = {};
	rs.Fuel = {};
	rs.Uranium = {};
	
	local foci = {};
	
	for i=1,#Game.lowCells do
		rs.Minerals[i] = 0;
		rs.Fuel[i] = 0;
		rs.Uranium[i] = 0;
	
		local cell = Game.lowCells[i];
		local good = true;
		for j=1,#foci do
			local dist = QuadraticDistance(cell.center,foci[j].center);
			if (dist < 0.15) then
				good = false;
				break;
			end;
		end;
		if (good) then
			table.insert(foci, cell);
		end;
	end;
	Game.Log("Identified "..#foci.." resource hotspots");
	
	for j=1,#foci do
		local f = foci[j];
		rs.Uranium[f.index] = ResourceFactor(f)*6;
		--always at least 5 neighbors
		rs.Minerals[f.neighbors[1].index] = ResourceFactor(f.neighbors[1])*12;
		rs.Minerals[f.neighbors[2].index] = ResourceFactor(f.neighbors[2])*12;
		rs.Fuel[f.neighbors[3].index] = ResourceFactor(f.neighbors[3])*12;
		rs.Fuel[f.neighbors[4].index] = ResourceFactor(f.neighbors[4])*12;
		rs.Fuel[f.neighbors[5].index] = ResourceFactor(f.neighbors[5])*12;
		
		--PropagateResource(rs, f, 0 );
		
		
	end;

	return rs;
end;


