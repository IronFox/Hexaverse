#ifndef monitoredProcessH
#define monitoredProcessH

#include <general/timer.h>
#include <global_string.h>

class MonitoredProcess
{
private:
	PROCESS_INFORMATION	infoOut;
	Timer::Time		lastCheck = timer.Now();
	bool			isStarted = false;
	String			parameters,
					executablePath,
					workingDirectory;

	bool			Restart();
	void			Flush();
public:
	bool			IsStarted() const {return isStarted;}
	bool			Start(const String&workingDirectory, const String&executablePath, const String&parametersWithoutExecutableName);
	void			Check();
	void			Terminate();
};








#endif
