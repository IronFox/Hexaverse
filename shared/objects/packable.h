#pragma once

#include <general/enumeration.h>
#include <interface/read_stream.h>
#include <container/array.h>


namespace D=DeltaWorks;

namespace Objects
{
	class Packable
	{
	public:
		CONSTRUCT_ENUMERATION3(Target,Owner,Ally,Hostile);

		virtual void Pack(D::IWriteStream&s,Target target) const=0;
		virtual void Unpack(D::IReadStream&s)=0;
		virtual void ToSavegame(D::IWriteStream&s) const=0;
		virtual void FromSavegame(D::IReadStream&s)=0;

		void PackToArray(D::Array<BYTE>&a,Target target) const;
		void UnpackFromArray(const D::Array<BYTE>&a);


	};

}				
