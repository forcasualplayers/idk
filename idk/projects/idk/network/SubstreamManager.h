#pragma once
#include <idk.h>
#include <meta/tuple.inl>

namespace idk
{
	class ConnectionManager;
	class ClientConnectionManager;
	class ServerConnectionManager;

	using SubstreamTypes = std::tuple<
		class EventManager,
		class GhostManager,
		class MoveManager,
		class DataBlockManager
	>;

	class BaseSubstreamManager
	{
	public:
		void SetConnectionManager(ConnectionManager* man);
		void NetworkFrameStart();
		void NetworkFrameEnd()  ;

		virtual ~BaseSubstreamManager() = default;
		virtual size_t GetManagerType() const = 0;
		virtual void SubscribeEvents(ClientConnectionManager&) = 0;
		virtual void SubscribeEvents(ServerConnectionManager&) = 0;
	protected:
		ConnectionManager* connection_manager = nullptr;
		vector<function<void()>> frame_start_functions;
		vector<function<void()>> frame_end_functions;

		template<typename Subscriber, typename ... Objects>
		void OnFrameStart(void(Subscriber::*)(span<Objects>...));
		template<typename Subscriber, typename ... Objects>
		void OnFrameEnd(void(Subscriber::*)(span<Objects>...));
	};

	template<typename T>
	class SubstreamManager
		: public BaseSubstreamManager
	{
	public:
		static_assert(index_in_tuple_v<T, SubstreamTypes> != std::tuple_size_v<SubstreamTypes>, "Please register Substream Manager in SubstreamManager.h/SubstreamTypes!");
		static constexpr auto type = index_in_tuple_v<T, SubstreamTypes>;
		size_t GetManagerType() const final { return type; }
	};

}