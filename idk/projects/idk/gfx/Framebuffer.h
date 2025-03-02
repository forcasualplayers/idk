#pragma once
#include <idk.h>
#include <res/Resource.h>
#include <res/ResourceHandle.h>
namespace idk
{
	enum class StoreOp
	{
		eDontCare,
		eStore
	};
	enum class LoadOp
	{
		eDontCare,
		eClear,
		eLoad
	};
	enum AttachmentType
	{
		eColor,
		eDepth,
		eStencil,
		eDepth3D,
		eSizeAT
	};

	inline AttachmentType& operator++(AttachmentType& type)
	{
		type = s_cast<AttachmentType>(s_cast<int>(type) + 1);
		return type;
	}
	struct Attachment
	{
		LoadOp  load_op{};
		StoreOp store_op{};
		LoadOp  stencil_load_op{};
		StoreOp stencil_store_op{};
		bool own_buffer = false;
		RscHandle<Texture> buffer;
		RscHandle<Texture> operator*()const { return buffer; }
		operator RscHandle<Texture>()const { return buffer; }
		const RscHandle<Texture>* operator->()const { return &buffer; }
		Attachment() = default;
		Attachment(const Attachment&) = delete;
		Attachment(Attachment&& rhs) :own_buffer{ rhs.own_buffer }, buffer{ rhs.buffer }
		{
			rhs.own_buffer = false;
			rhs.buffer = {};
		}
		Attachment& operator=(const Attachment&) = delete;
		Attachment& operator=(Attachment&& ) = default;
		virtual ~Attachment();
	};

	class FrameBuffer : public Resource<FrameBuffer>
	{
	public:
		vector<unique_ptr<Attachment>> attachments{};
		unique_ptr<Attachment> depth_attachment{}, stencil_attachment{};
		uvec2 size{};
		size_t num_layers{};
		bool HasDepthAttachment()const { return s_cast<bool>(depth_attachment); }
		const Attachment& DepthAttachment()const { return *depth_attachment; }
		const Attachment& GetAttachment(size_t index)const { return *attachments[index]; }
		size_t NumColorAttachments()const {return attachments.size();}
		size_t NumAttachments()const;
		size_t NumLayers()const;
		uvec2 Size()const { return size; };
		uvec2 Size(uvec2 new_size)const { return new_size=size; };


		FrameBuffer() = default;
		FrameBuffer(FrameBuffer&&) = default;
		FrameBuffer& operator=(FrameBuffer&&) = default;
		virtual ~FrameBuffer() = default;
	};

}