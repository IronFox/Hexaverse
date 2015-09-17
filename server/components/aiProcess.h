#ifndef aiProcessH
#define aiProcessH

#include "../shared/uniqueID.h"

namespace AIProcess
{
	void			Check();
	void			Begin(const CryptographicID&accessToken);
	void			End();
}





#endif
