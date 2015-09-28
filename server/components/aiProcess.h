#ifndef aiProcessH
#define aiProcessH

#include "../../shared/uniqueID.h"
#include "../../shared/monitoredProcess.h"

namespace AIProcess
{
	void			Check();
	void			Begin(const CryptographicID&accessToken);
	void			End();
}





#endif
