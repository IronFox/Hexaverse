#ifndef versionH
#define versionH
	#include "buildCounter.h"

	#define MAJOR_VERSION		0
	#define MAJOR_VERSION_STR	"0"
	#define MINOR_VERSION		6
	#define MINOR_VERSION_STR	"6"
	#define MICRO_VERSION		7
	#define MICRO_VERSION_STR	"7"


	#define PRODUCT_VERSION	 MAJOR_VERSION_STR "." MINOR_VERSION_STR "." MICRO_VERSION_STR "." BUILD_COUNTER_STR

	#define PRODUCT_NAME "Hexaverse"

#endif
