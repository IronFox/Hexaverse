#include <global_root.h>
#include "aiProcess.h"
#include <global_string.h>
#include <uniqueID.h>
#include <general/timer.h>
#include <io/monitoredProcess.h>
#include <io/file_system.h>
#include <globalEvents.h>
#include <logEvent.h>
#include <startProcess.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace AIProcess
{
	namespace detail
	{
		struct Instance
		{
			CryptographicID			accessToken;
			bool					isPlayer = false;
			MonitoredProcess		process;

			/**/					Instance():process(true)	{}
		};

		std::mutex	instanceLock;
		std::map<std::string, std::unique_ptr<Instance> >	instances;	//keyed by full token string
		index_t	eventKey = InvalidIndex;

		std::string	Key(const CryptographicID&token)
		{
			return std::string(token.ToFullString().c_str());
		}

		void	Start(Instance&instance, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword, const std::function<String()>&asyncGetExtraParameters)
		{
			PathString	path,
						workingDirectory = L"..\\client\\";
				path = Process::LocateProjectExecutable(workingDirectory,
					#ifdef _DEBUG
						L"Visual Client"
					#else
						L"Console Client"
					#endif
				);


			try
			{
				bool window = false;
				#ifdef _DEBUG
					window = true;
				#endif

				const CryptographicID accessToken = instance.accessToken;
				instance.process.Start(workingDirectory,path,[accessToken,serverBasePort,asyncGetServerPassword,asyncGetExtraParameters]()
				{
					String extra;
					if (asyncGetExtraParameters)
						extra = asyncGetExtraParameters();
					char parameterBuffer[32768];
					sprintf_s(parameterBuffer,"aiAccessToken='%s' password='%s' udpListen=false serverURL=localhost:%u %s",accessToken.ToFullString().c_str(), asyncGetServerPassword().c_str(),serverBasePort,extra.c_str());
					return FileSystem::StringToPath(parameterBuffer);
				},window);
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::error, ex.what());
				if (IsDebuggerPresent())
					DebugBreak();
			}
		}

		void	BeginInstance(const CryptographicID&accessToken, bool isPlayer, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword, const std::function<String()>&asyncGetExtraParameters)
		{
			std::lock_guard<std::mutex> guard(instanceLock);
			auto&slot = instances[Key(accessToken)];
			if (slot && slot->process.IsRunning())
				return;
			if (!slot)
			{
				slot.reset(new Instance());
				slot->accessToken = accessToken;
				slot->isPlayer = isPlayer;
			}
			Start(*slot,serverBasePort,asyncGetServerPassword,asyncGetExtraParameters);
		}

		void	EndWhere(const std::function<bool(const Instance&)>&predicate)
		{
			std::lock_guard<std::mutex> guard(instanceLock);
			for (auto it = instances.begin(); it != instances.end();)
			{
				if (predicate(*it->second))
				{
					it->second->process.SetAutoRestart(false);
					it->second->process.Terminate();
					it = instances.erase(it);
				}
				else
					++it;
			}
		}

		void EventEnd()
		{
			EndWhere([](const Instance&){return true;});
		}

		void RegisterTermination()
		{
			if (eventKey == InvalidIndex)
				eventKey = GlobalEvents::applicationTerminate.Register(EventEnd);
		}
	}


	void Begin( const CryptographicID&accessToken, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword )
	{
		detail::BeginInstance(accessToken,false,serverBasePort,asyncGetServerPassword,nullptr);
		detail::RegisterTermination();
	}

	void BeginPlayer(const CryptographicID&accessToken, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword, const std::function<String()>&asyncGetExtraParameters)
	{
		detail::BeginInstance(accessToken,true,serverBasePort,asyncGetServerPassword,asyncGetExtraParameters);
		detail::RegisterTermination();
	}

	void EndOne(const CryptographicID&accessToken)
	{
		const std::string key = detail::Key(accessToken);
		detail::EndWhere([&key](const detail::Instance&i){return detail::Key(i.accessToken) == key;});
	}

	void End()
	{
		detail::EventEnd();
	}

	void EndPlayers()
	{
		detail::EndWhere([](const detail::Instance&i){return i.isPlayer;});
	}
}
