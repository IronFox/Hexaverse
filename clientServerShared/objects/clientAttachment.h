#pragma once

#include <memory>

namespace Objects
{
	class ClientAttachment
	{
	public:
		virtual		~ClientAttachment()	{}
	};
	typedef std::shared_ptr<ClientAttachment>	PClientAttachment;
}
