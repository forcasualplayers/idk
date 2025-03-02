#pragma once
#include <idk.h>

#include <vulkan/vulkan.hpp>
namespace idk::vkn
{
	class VulkanView;
	class VulkanWin32GraphicsSystem;
	class VulkanResourceManager;

	struct VulkanRscBase
	{
		VulkanRscBase() = default;
		VulkanRscBase(VulkanRscBase&&)noexcept = default;
		//destroys/releases the object immediately (it is possible for this function to not be called before destroying via destructor)
		virtual void* Data() = 0;
		virtual void Destroy()=0;
		virtual ~VulkanRscBase()=default;
	};
	template<typename T>
	struct VulkanRscDel : VulkanRscBase,vk::UniqueHandle<T, vk::DispatchLoaderDefault>
	{
		using Base = vk::UniqueHandle<T, vk::DispatchLoaderDefault>;
		using Base::Base;
		VulkanRscDel(Base&& base)noexcept :Base{ std::move(base) } {}
		VulkanRscDel& operator=(VulkanRscDel&& rhs)
		{
			auto& tmp = s_cast<Base&>(*this);
			std::swap(tmp, s_cast<Base&>(rhs));
			return *this;
		}
		VulkanRscDel& operator=(Base&& rhs)
		{
			*this = VulkanRscDel{ std::move(rhs) };
			return *this;
		}
		void* Data() override
		{
			return *reinterpret_cast<void**>(&Base::get());
		}
		void Destroy() override
		{
			this->reset();
		}
	};
	template<typename ManagedT>
	struct ManagedRscDel : VulkanRscBase, ManagedT
	{
		using Base = ManagedT;
		using Base::Base;
		ManagedRscDel(Base&& base)noexcept :Base{ std::move(base) } {}
		void* Data() override
		{
			return nullptr;
		}
		ManagedRscDel& operator=(ManagedRscDel&& rhs)
		{
			auto& tmp = s_cast<Base&>(*this);
			std::swap(tmp, s_cast<Base&>(rhs));
			return *this;
		}
		ManagedRscDel& operator=(Base&& rhs)
		{
			*this = ManagedRscDel{ std::move(rhs) };
			return *this;
		}
		void Destroy() override
		{
			this->reset();
		}
	};
	namespace impl
	{
		VulkanResourceManager* GetRscManager();
	}
	template<typename UniqueT>
	struct ManagedRsc : UniqueT
	{
		using Base = UniqueT;
		using Base::Base;
		ManagedRsc() = default;
		ManagedRsc(Base && base)noexcept :Base{ std::move(base) } {}
		ManagedRsc(ManagedRsc<UniqueT> && base)noexcept :Base{ std::move(base) } {}

		ManagedRsc& operator=(ManagedRsc && rhs)
		{
			auto& tmp = s_cast<Base&>(*this);
			std::swap(tmp, s_cast<Base&>(rhs));
			return *this;
		}
		ManagedRsc& operator=(Base && rhs)
		{
			ManagedRsc tmp{ std::move(rhs) };
			//std::swap(static_cast<Base&>(*this),static_cast<Base&>(tmp)) ;
			return *this = std::move(tmp);
		}

		//void Destroy() override
		//{
		//	reset();
		//}
		~ManagedRsc();
	};

	template<typename T>
	struct VulkanRsc : vk::UniqueHandle<T, vk::DispatchLoaderDefault>
	{
		using Base = vk::UniqueHandle<T, vk::DispatchLoaderDefault>;
		using Base::Base;
		VulkanRsc() = default;
		VulkanRsc(Base&& base)noexcept :Base{ std::move(base) } {}
		VulkanRsc(VulkanRsc<T>&& base)noexcept :Base{ std::move(base) } {}

		VulkanRsc& operator=(VulkanRsc&& rhs)
		{
			auto& tmp = s_cast<Base&>(*this);
			std::swap(tmp, s_cast<Base&>(rhs));
			return *this;
		}
		VulkanRsc& operator=(Base&& rhs)
		{
			VulkanRsc tmp{ std::move(rhs) };
			//std::swap(static_cast<Base&>(*this),static_cast<Base&>(tmp)) ;
			return *this = std::move(tmp);
		}

		//void Destroy() override
		//{
		//	reset();
		//}
		~VulkanRsc();
	};
	class VulkanResourceManager
	{
	public:
		using ptr_t = unique_ptr<VulkanRscBase>;
		/*void RegisterRsc(shared_ptr<VulkanRscBase> rsc)
		{
			managed.emplace_back(rsc);
		}*/
		void QueueToDestroy(ptr_t obj_to_destroy);
		void ProcessQueue(uint32_t frame_index);
		/*
		//Finds and queues all the resources with no existing references.
		void ProcessSingles(uint32_t frame_index)
		{
			auto& queue = destroy_queue[frame_index];
			//Destroy the stuff that have already waited for 1 full frame cycle
			queue.clear();//Assumes that the shared_ptrs queued are ready for destruction
			auto itr = managed.begin(), end = managed.end();
			for (; itr < end; ++itr)
			{
				//Last owner, assume no weak_ptrs to be locked in another thread at this time
				if (itr->use_count() == 1)
				{
					//swap to "back"
					std::swap(*itr, (*--end));
				}
			}
			//Queue the next batch of stuff to be destroyed.
			queue.insert(queue.end(), end, managed.end());
			managed.resize(end - managed.begin());//trim it down
		}
		*/
		void DestroyAll();
	private:
		vector<ptr_t> managed;
		hash_table<uint32_t, vector<ptr_t>> destroy_queue;
	};

	template<typename T>
	VulkanRsc<T>::~VulkanRsc()
	{
		//If valid pointer
		if (*this)
		{
			VulkanResourceManager* manager = impl::GetRscManager();
			manager->QueueToDestroy(std::make_unique<VulkanRscDel<T>>(s_cast<Base&&>(std::move(*this))));

		}
	}

	template<typename UniqueT>
	ManagedRsc<UniqueT>::~ManagedRsc()
	{
		//If valid pointer
		if (*this)
		{
			VulkanResourceManager* manager = impl::GetRscManager();
			manager->QueueToDestroy(std::make_unique<ManagedRscDel<UniqueT>>(s_cast<Base&&>(std::move(*this))));

		}
	}

}