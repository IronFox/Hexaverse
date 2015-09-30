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

	NETSTRUCT5(GridDelta
		, NET(UINT32), nodeOffset
		, NET(UINT32), nodeCount
		, ::Protocol::TypeDeltaArray, data0
		, ::Protocol::StructCountDeltaArray, data1
		, ::Protocol::TechniteFactionDeltaArray, data2
	);

	NETSTRUCT3(GridNode
		, NETA(UINT32), neighbors
		, NET(TVec3<>), stackBase
		, NET(TVec3<>), stackDirection
	);

	NETSTRUCT3(GridConfig
		, NET(float), heightPerLayer
		, NET(int), numLayersPerStack
		, NETA(BYTE), matterYieldByContentType
	);


	NETSTRUCT2(NodeChunk
		, NET(bool), isLast
		, NETA(GridNode), nodes
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
			Authenticate,			//c2s: key-string
			TechniteStateChunk,     //s2c: TechniteStateChunk
			InstructTechnites,      //s2c: <signal>
			TechniteInstructionChunk, //c2s: TechniteInstructionChunk

			NodeChunk,      //s2c: NodeChunk - completed before SessionBegin is sent
			GridConfig,		//s2c: GridConfig
			GridDelta,		//s2c: GridDelta
			WorldInfo,		//s2c: WorldInfo

			Count     //must remain last
		};
	}




}




#endif
