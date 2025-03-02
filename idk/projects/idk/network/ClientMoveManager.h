#pragma once
#include <network/GameConfiguration.h>
#include <network/SubstreamManager.h>

namespace idk
{
	class GhostMessage;
	class ControlObjectMessage;

	class ClientMoveManager
		: public SubstreamManager<MoveManager>
	{
	public:
		void SubscribeEvents(ClientConnectionManager& client) override;
		void SubscribeEvents(ServerConnectionManager& server) override;
		void SendMoves(span<ElectronView> views);

	private:
		void OnControlObject(ControlObjectMessage& msg);
	};
}