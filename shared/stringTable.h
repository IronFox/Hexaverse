#ifndef stringTableH
#define stringTableH

#include <stdint.h>

typedef uint8_t ConstNameRootType;

enum class ConstName : ConstNameRootType	//DO NOT WRITE TO PERSISTENT STORAGE!!!!!! Order and composition may change between versions
{
	Empty,

	Turret_Terraformer,
	Turret_Radar,
	Turret_Mechanic,
	Turret_Artillery,
	Turret_Cannon,
	Turret_Surveyor,
	Turret_EMP,
	Turret_ProtonCannon,
	Turret_SkyHub,
	Turret_StarPort,
	Turret_Launcher,
	Turret_NukeLauncher,
	Turret_MunitionTransporter,
	Turret_Ragnarok,
	
	VehicleBase_HeavyBase,
	VehicleBase_HoverBase,
	VehicleBase_CatBase,
	

	Job_BuildHorizontalWall,
	Job_BuildWallUp,
	Job_BuildRoad,
	Job_Construct,
	Job_DigDown,
	Job_DigHorizontal,
	Job_AlterTerrain,
	Job_Transfer,
	Job_Repair,
	Job_ClearAbove,
	Job_Sequence,

	Type_Marker,
	Type_Vehicle,
	Type_Connector,
	Type_Pylon,
	Type_NexusPort,
	Type_Generator,
	Type_ReactorCore,
	Type_VehiclePlant,
	Type_Mine,
	Type_MunitionFactory,
	Type_FuelContainer,
	Type_MineralContainer,
	Type_UraniumStorage,
	Type_MunitionStorage,
	Type_CannonTower,
	Type_ArtilleryTower,
	Type_LauncherTower,
	Type_NukeLauncherTower,
	Type_MechanicTower,
	Type_EMPTower,
	Type_RadarTower,
	Type_VehicleStore,
	Type_SkyPort,
	Type_SkyHub,
	Type_SkyGuardian,
	Type_Ragnarok,
	Type_StarPort,

	Resource_Fuel,
	Resource_Minerals,
	Resource_Uranium,
		


	NumTypes,
};


static_assert(sizeof(ConstName) == sizeof(ConstNameRootType), "compiler broken");


#endif
