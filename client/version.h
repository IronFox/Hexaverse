#ifndef versionH
#define versionH
	#include "buildCounter.h"

	#define MAJOR_VERSION		0
	#define MAJOR_VERSION_STR	"0"
	#define MINOR_VERSION		4
	#define MINOR_VERSION_STR	"4"
	#define MICRO_VERSION		4
	#define MICRO_VERSION_STR	"4"


	#define PRODUCT_VERSION	 MAJOR_VERSION_STR "." MINOR_VERSION_STR "." MICRO_VERSION_STR "." BUILD_COUNTER_STR

	#define PRODUCT_NAME "Hexaverse"

#endif
