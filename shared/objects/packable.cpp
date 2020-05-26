#include "packable.h"

using namespace DeltaWorks;

namespace Objects
{
	void Packable::Pack(D::Array<BYTE>&a,Target target) const
	{
		using Serialization::Serialize;
		SerialSizeScanner scanner;
		Pack(scanner,target);
		a.SetSize(scanner.GetTotalSize());
		if (scanner.GetTotalSize())
		{
			MemWriteStream	writer(a.GetPointer(),scanner.GetTotalSize());
			Pack(writer,target);
		}
	} 

	void Packable::Unpack(const D::Array<BYTE>&a)
	{
		MemReadStream s(a.GetPointer(),a.GetLength());
		Unpack(s);
	}


}

