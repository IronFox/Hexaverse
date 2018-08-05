#include <global_root.h>
#include "udpBroadcast.h"
#include <protocol.h>
#include <globalEvents.h>
#include "../../client/version.h"
#include "serverLogEvent.h"

#include "state/networkState.h"



namespace UDPBroadCast
{

	SOCKET	broadcastSocket = INVALID_SOCKET;


	Ctr::Buffer0<sockaddr_in>	everyone;
	Timer::Time		lastAdapterQuery, lastBroadcast;

	Protocol::THostBroadcastPacket	packet;
	Protocol::THostToManagerPacket	managerPacket;
	static sockaddr_in managerAddr;
	static bool doBroadcast = false;


	void UpdateOutAdapters(Ctr::BasicBuffer<sockaddr_in>&out)
	{
		out.Clear();



		/*
		note 1:
			use HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography\MachineGuid for unique machine id
		note 2:
			enumerate adapters and send one broadcast for each adapter&subnetmask | ~subnetmask
		*/


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

	void	Destroy()
	{
		if (broadcastSocket != INVALID_SOCKET)
		{
			closesocket(broadcastSocket);
			broadcastSocket = INVALID_SOCKET;
		}
	}



	void		Create(UINT16 basePort, const TUniqueID&serverID)
	{
		GlobalEvents::applicationTerminate << Destroy;
		packet.hostID = serverID;
		packet.serverBasePort = basePort;
		managerPacket.hostID = serverID;
		managerPacket.serverBasePort = basePort;
		{
			const char var[] = SERVER_NAME;
			const size_t len = sizeof(var);
			ASSERT_LESS__(len,sizeof(packet.version));

			memcpy(packet.version,var,len);
		}


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


		broadcastSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		ASSERT_NOT_EQUAL__(broadcastSocket,INVALID_SOCKET);
		SOCKADDR_IN me;
		memset(&me,0,sizeof(me));
		me.sin_family=AF_INET;
		me.sin_port=0;
		me.sin_addr.s_addr=ADDR_ANY;
		int rc=::bind(broadcastSocket,(SOCKADDR*)&me,sizeof(SOCKADDR_IN));
		if (rc != 0)
		{
			closesocket(broadcastSocket);
			broadcastSocket = INVALID_SOCKET;
			FATAL__("Unable to bind UDP broadcast socket.");	//this should never happen (random port)
			return;
		}
		int so_broadcast = TRUE;
		rc = setsockopt(broadcastSocket,SOL_SOCKET,SO_BROADCAST,(const char*)&so_broadcast,sizeof so_broadcast);
		ASSERT_EQUAL__(rc,0);

		memset(&managerAddr,0,sizeof(managerAddr));
		managerAddr.sin_family=AF_INET;
		managerAddr.sin_addr.s_addr= htonl( INADDR_LOOPBACK );
		managerAddr.sin_port = htons(MANAGER_UNICAST_PORT);

	}


	bool		Start()
	{
	

		UpdateOutAdapters(everyone);

		doBroadcast = true;
		{
			struct sockaddr_in bound;
			int addrlen = sizeof(bound);
			if(getsockname(broadcastSocket, (struct sockaddr *)&bound, &addrlen) == 0 && addrlen == sizeof(bound))
			{
				auto p = ntohs(bound.sin_port);
				LogEvent(EventClass::Preset::network,"UDP broadcast online, broadcasting from port "+String(p));
			}
			else
				LogEvent(EventClass::Preset::network,"UDP broadcast online, broadcasting from unknown port");
		}


		lastBroadcast = lastAdapterQuery = timer.Now();

		return true;
	}


	void		Stop()
	{
		doBroadcast = false;
	}

	bool		IsStarted()
	{
		return doBroadcast;
	}


	void	Update()
	{
		if (timer.CheckIfOutdatedAndUpdate(lastBroadcast,1))
		{

			managerPacket.flags = 0;
			if (!Network::SessionIsStarted() || Network::SessionIsSuspended())
				managerPacket.flags |= managerPacket.SessionClosed;
			if (Network::RequiresHeartbeats())
				managerPacket.flags |= managerPacket.RequireHeartbeats;
			sendto(broadcastSocket,(const char*)&managerPacket,sizeof(managerPacket),0,(const sockaddr*)&managerAddr,(int)sizeof(sockaddr_in));

			if (doBroadcast)
			{
				if (timer.CheckIfOutdatedAndUpdate( lastAdapterQuery,5))
				{
					UpdateOutAdapters(everyone);
					lastAdapterQuery = timer.Now();
				}

				foreach (everyone,someone)
				{
					sendto(broadcastSocket,(const char*)&packet,sizeof(packet),0,(const sockaddr*)someone,(int)sizeof(sockaddr_in));
				}
			}
		}
	}

	bool		PacketIsDone()
	{
		return true;
	}

}


