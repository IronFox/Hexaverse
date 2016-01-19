#include <global_root.h>
#include "udpBroadcast.h"
#include "../../shared/protocol.h"
#include "../../client/version.h"
#include "serverLogEvent.h"

#include "state/networkState.h"


namespace UDPBroadCast
{

	SOCKET	broadcastSocket = INVALID_SOCKET;


	Buffer0<sockaddr_in>	everyone;
	Timer::Time		lastAdapterQuery, lastBroadcast;

	Protocol::THostBroadcastPacket	packet;



	void UpdateOutAdapters(BasicBuffer<sockaddr_in>&out)
	{
		out.Clear();

		IP_ADAPTER_ADDRESSES*addr = NULL;
		ULONG size = 0;
		const ULONG flags = GAA_FLAG_INCLUDE_TUNNEL_BINDINGORDER | GAA_FLAG_INCLUDE_PREFIX;	//GAA_FLAG_SKIP_DNS_SERVER
		ULONG rc = GetAdaptersAddresses(AF_INET,flags,NULL,addr,&size);
		if (rc == ERROR_BUFFER_OVERFLOW)
		{
			addr = (IP_ADAPTER_ADDRESSES*) malloc(size);
			rc = GetAdaptersAddresses(AF_INET,flags,NULL,addr,&size);
		}


		IP_ADAPTER_ADDRESSES*current = addr;
		while (current)
		{
			//const SOCKET_ADDRESS& addr = current->FirstAnycastAddress->Address;
			//Console::LockedMemPrint("single: ",current->FirstUnicastAddress->Address.lpSockaddr->sa_data,14);
			//Console::LockedMemPrint("multi: ",current->FirstMulticastAddress->Address.lpSockaddr->sa_data,14);
			PIP_ADAPTER_PREFIX_XP prefix = current->FirstPrefix;
			count_t cnt = 0;
			while (prefix)
			{
				//std::cout << " " << TCP::ToString((const sockaddr_storage&)*prefix->Address.lpSockaddr);
				cnt++;
				prefix = prefix->Next;
			}
			if (cnt >= 3)
			{
				prefix = current->FirstPrefix;
				sockaddr addr;
				count_t at = 0;
				while (prefix)
				{
					//std::cout << " " << TCP::ToString((const sockaddr_storage&)*prefix->Address.lpSockaddr);
					if (at == cnt-3)
						addr = *prefix->Address.lpSockaddr;
					//Console::LockedMemPrint("prefix: ",prefix->Address.lpSockaddr->sa_data,14);
					at++;
					prefix = prefix->Next;
				}
				//std::cout << std::endl;



				sockaddr_in&everyone = out.Append();
				ASSERT_EQUAL__(sizeof(sockaddr_in),sizeof(sockaddr));
				memcpy(&everyone,&addr,sizeof(addr));
				everyone.sin_port = htons(BROADCAST_PORT);
				//std::cout << TCP::ToString((const sockaddr_storage&)everyone) << std::endl;

			/*	char buffer[INET_ADDRSTRLEN];
				inet_ntop( AF_INET, &everyone.sin_addr, buffer, INET_ADDRSTRLEN );
				Console::LockedPrintLine(buffer);
	*/
			}
			current = current->Next;
		}
		if (addr)
			free(addr);

 

		//Sleep(1000);

	}


	void		Create(UINT16 basePort)
	{
	
		packet.hostID = Network::serverID;
		packet.serverBasePort = basePort;
		{
			const char var[] = SERVER_NAME;
			const size_t len = sizeof(var);
			ASSERT_LESS__(len,sizeof(packet.version));

			memcpy(packet.version,var,len);
		}

	}


	bool		Start()
	{
		#ifdef _WIN32
			//Start up Winsock…
			WSADATA wsadata;

			int error = WSAStartup(0x0202, &wsadata);
			if (error)	//Did something happen?
			{
				FATAL__("Unable to start winsock");
			}

			if (wsadata.wVersion != 0x0202)//Did we get the right Winsock version?
			{
				FATAL__("WSA version mismatch");
			}
		#endif


		/*
		note 1:
			use HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography\MachineGuid for unique machine id
		note 2:
			enumerate adapters and send one broadcast for each adapter&subnetmask | ~subnetmask
		*/


		broadcastSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		ASSERT_NOT_EQUAL__(broadcastSocket,INVALID_SOCKET);
		SOCKADDR_IN me;
		memset(&me,0,sizeof(me));
		me.sin_family=AF_INET;
		UINT16 port = packet.serverBasePort + PortOffset::BroadcastFrom;
		me.sin_port=htons(packet.serverBasePort);
		me.sin_addr.s_addr=ADDR_ANY;
		int rc=::bind(broadcastSocket,(SOCKADDR*)&me,sizeof(SOCKADDR_IN));
		if (rc != 0)
		{
			closesocket(broadcastSocket);
			broadcastSocket = INVALID_SOCKET;
			LogEvent(EventType::SessionRelated,"Warning: Unable to bind UDP broadcast socket. Chances are, a server is already running.");
			return false;
		}
		LogEvent(EventType::SessionRelated,"UDP broadcast online, broadcasting from port "+String(port));
		int so_broadcast = TRUE;
		rc = setsockopt(broadcastSocket,SOL_SOCKET,SO_BROADCAST,(const char*)&so_broadcast,sizeof so_broadcast);
		ASSERT_EQUAL__(rc,0);

		UpdateOutAdapters(everyone);
		lastBroadcast = lastAdapterQuery = timer.Now();

		return true;
	}


	void	Terminate()
	{
		if (broadcastSocket != INVALID_SOCKET)
		{
			closesocket(broadcastSocket);
			broadcastSocket = INVALID_SOCKET;
		}
	}


	bool		IsStarted()
	{
		return broadcastSocket != INVALID_SOCKET;
	}


	void	Update()
	{

		if (broadcastSocket == INVALID_SOCKET)
			return;
		if (timer.GetSecondsSince( lastAdapterQuery) > 5)
		{
			UpdateOutAdapters(everyone);
			lastAdapterQuery = timer.Now();
		}
		if (timer.GetSecondsSince(lastBroadcast) > 1)
		{
			//strcpy(packet.gameName,"Terra Nova");
			//packet.numClients = (UINT16)State::clients.Count();
			//packet.numPlayers = (UINT16)State::activePlayers.Count();
			//packet.roundNumber = (UINT32)RuntimeConfig::roundNumber;
			//packet.serverMode = (BYTE)State::mode;
			foreach (everyone,someone)
			{
				sendto(broadcastSocket,(const char*)&packet,sizeof(packet),0,(const sockaddr*)someone,(int)sizeof(sockaddr_in));
			}
			lastBroadcast = timer.Now();
		}
	}
}


