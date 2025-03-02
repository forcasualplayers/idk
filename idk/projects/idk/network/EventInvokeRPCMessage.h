#pragma once
#include <idk.h>
#include <network/network.h>
#include <network/Message.h>
#include <res/ResourceHandle.h>

namespace idk
{
	class EventInvokeRPCMessage
		: public Message
	{
	public:
		struct Param
		{
			int size = 0;
			vector<unsigned char> buffer;
		};
		struct Data
		{
			NetworkID invoke_on_id = 0;
			//char method_name[64];
			int method_id = 0;
			unsigned param_count = 0;
			vector<Param> param_buffer;
		} payload;

		template <typename Stream>
		bool Serialize(Stream& stream)
		{
			serialize_int(stream, payload.invoke_on_id, 0, 4096);
			serialize_int(stream, payload.method_id, -1, 255);
			serialize_uint32(stream, payload.param_count);
			payload.param_buffer.resize(payload.param_count);

			for (auto& elem : payload.param_buffer)
			{
				serialize_int(stream, elem.size, 0, 0x7FFF);
				elem.buffer.resize(elem.size);
				serialize_bytes(stream, (uint8_t*)elem.buffer.data(), elem.size);
			}

			return true;
		}

		NETWORK_MESSAGE_VIRTUAL_SERIALIZE_FUNCTIONS()
	};
}