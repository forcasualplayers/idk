#pragma once
#include <limits>
#include <network/network.h>
#include <network/Message.h>

namespace idk
{
	class GhostAcknowledgementMessage
		: public Message 
	{
	public:
		SeqNo base_ack;
		unsigned ack_field;

		template <typename Stream>
		bool Serialize(Stream& stream)
		{
			serialize_int(stream, base_ack.value, 0, SeqNo::max_value);
			serialize_int(stream, ack_field, 0, std::numeric_limits<int>::max());
			return true;
		}

		NETWORK_MESSAGE_VIRTUAL_SERIALIZE_FUNCTIONS()
	};
}