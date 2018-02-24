#ifndef timeLapseH
#define timeLapseH

#ifdef HAS_WINDOW

namespace InputControl
{
	extern bool saveScreenshot;
}

namespace TimeLapse
{
	extern bool enabled;

	extern index_t	runningNumber;
	extern float	frameDelaySeconds;
	extern PathString	basePath, customBasePath;

}

#endif



#endif
