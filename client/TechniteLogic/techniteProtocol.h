#ifndef techniteProtocolH
#define techniteProtocolH


#include <iphlpapi.h>
#include <net/tcp.h>
#include <net/containers.h>
//#include "grid.h"
//#include "objects.h"
//#include "stringTable.h"
//#include "technite.h"


#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Rpcrt4.lib")
#pragma comment(lib, "IPHLPAPI.lib")





#include "../../shared/protocol.h"
#include "../../shared/sphereGrid.h"

namespace TechniteProtocol
{
	//NETSTRUCT2(TechniteResources
	//	,NET(BYTE), energy
	//	,NET(BYTE), matter
	//);

	//NETSTRUCT4(TechniteState
	//	, NET(UINT32), location
	//	, TechniteResources, resources
	//	, NET(BYTE), taskResult
	//	, NET(BYTE), state
	//);


	//byte-wise identical to Protocol::TechniteChunk
	//NETSTRUCT2(TechniteStateChunk
	//	, NET(bool), isFirst
	//	, NETA(TechniteState), states	//max 32768
	//);


	//NETSTRUCT3(TechniteInstruction
	//	, NET(BYTE), nextTask
	//	, NET(BYTE), taskTarget
	//	, NET(BYTE), taskParameter
	//);

	//byte-wise identical to Protocol::TechniteInstructions
	//NETSTRUCT2(TechniteInstructionChunk
	//	, NET(UINT32), offset
	//	, NETA(TechniteInstruction), instructions	//actual max is 32768. Technite logic states 10000, which is fine, too
	//);

	NETSTRUCT4(GridDelta
		, NET(UINT32), nodeOffset
		, NET(UINT32), nodeCount
		, ::Protocol::TypeDeltaArray, data0
		, ::Protocol::TechniteFactionDeltaArray, data1
	);

	NETSTRUCT3(GridNode
		, NETA(UINT32), neighbors
		, NET(TVec3<>), stackBase
		, NET(TVec3<>), stackDirection
	);

	NETSTRUCT6(GridConfig
		, NET(float), heightPerLayer
		, NET(int), numLayersPerStack
		, NETA(BYTE), matterYieldByContentType
		, NETA(BYTE), matterDegradationTable
		, NETA(BYTE), initialTTLAtLayer
		, NETA(BYTE), energyYieldAtLayer
	);


	NETSTRUCT2(NodeChunk
		, NET(bool), isLast
		, NETA(GridNode), nodes
	);

	NETSTRUCT2(TechniteColor
		, NET(UINT32), index
		, NET(TVec3<BYTE>), color
	);

	NETSTRUCT2(TechniteColorChunk
		, NET(UINT32), offset
		, NETA(TechniteColor), colors
	);
//	static_assert(sizeof(TechniteColor)==7,"Struct alignment issue?");

	static_assert(sizeof(TVec3<BYTE>)==3,"Struct alignment issue?");


	enum GameObjectType
	{
		Vehicle,
		Building,
		Satellite
	};

	NETSTRUCT3(GameObjectID
		, NET(UINT32), location
		, NET(bool), isGhost
		, NET(BYTE), type
		);

	NETSTRUCT6(GameObject
		, GameObjectID, id
		, NET(UINT32), birthRound
		, NET(BYTE), height
		, NET(bool), isBroad
		, Package::NetString<128>, className
		, NET(bool), isMine
		);

	NETSTRUCT4(ControlMarker
		, NET(UINT32), location
		, NET(float), radius
		, NET(BYTE), typeIndex
		, NET(UINT32), birthRound
		);


	NETSTRUCT2(InstructTechnites
		, NET(UINT32), roundNumber
		, NET(UINT32), techniteSubRoundNumber
		);

	NETSTRUCT4(BeginSession
		, String, factionID
		, NET(BYTE), techniteGridID
		, NET(UINT32), roundNumber
		, NET(UINT32), techniteSubRoundNumber
		);


	NETSTRUCT2(TechniteDebugMessage
		, NET(UINT32), location
		, Package::NetString<512>, message
		);


	//NETSTRUCT2(SessionBegin
	//	, GridConfig, grid
	//	, NET(BYTE), myFactionGridID
	//);

	typedef NET(BYTE)	WorldInfo;	//coreContent


	namespace ChannelID
	{
		//c2s: client to server
		//s2c: server to client

		enum id_t
		{
			Unused,
			Ready,					//c2s: String: protocol version+name
			Error,					//s2c: String: message
			TechniteStateChunk,     //s2c: TechniteStateChunk
			InstructTechnites,      //s2c: InstructTechnites
			TechniteInstructionChunk, //c2s: TechniteInstructionChunk

			NodeChunk,      //s2c: NodeChunk - completed before SessionBegin is sent
			GridConfig,		//s2c: GridConfig
			GridDelta,      //s2c: GridDelta
			WorldInfo,		//s2c: WorldInfo
			BeginSession,	//s2c: BeginSession

			CreateControlMarker,	//s2c: ControlMarker
			CreateGameObject,		//s2c: GameObject
			RemoveControlMarker,	//s2c: UINT32 (compressed location)
			RemoveGameObject,       //s2c: RemoveGameObject

			RequestNextRound, //c2s: <signal>

			TechniteColorChunk,		//c2s: TechniteColorChunk
			TechniteDebugMessage,	//c2s: TechniteDebugMessage

			Count     //must remain last
		};
	}




}




#endif
