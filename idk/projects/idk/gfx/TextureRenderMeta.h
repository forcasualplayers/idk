#pragma once
#include <idk.h>
#include <util/enum.h>

namespace idk{

	ENUM(ColorFormat, char,
		R_8,
		R_16,
		R_32F,
		R_64F,
		Rint_8,
		Rint_16,
		Rint_32,
		Rint_64,
		RG_8,
		RGF_16,
		RGB_8,
		RGBA_8,
		RGBF_16,
		RGBF_32,
		RGBAF_16,
		RGBAF_32,
		BGRA_8,
		RUI_32,
		DEPTH_COMPONENT,
		DXT1,
		DXT3,
		DXT5,
		DXT1_A,
		SRGB,
		SRGBA,
		SRGB_DXT1,
		SRGB_DXT3,
		SRGB_DXT5,
		SRGBA_DXT1
	); //TODO remove the SRGB from this list
	inline bool IsSrgb(ColorFormat cf)
	{
		return
			(cf == ColorFormat::SRGB_DXT1) |
			(cf == ColorFormat::SRGBA_DXT1) |
			(cf == ColorFormat::SRGB_DXT3) |
			(cf == ColorFormat::SRGB_DXT5) |
			(cf == ColorFormat::SRGB) |
			(cf == ColorFormat::SRGBA)
			;
	}

	ENUM(UVMode, char,
		Repeat,
		MirrorRepeat,
		Clamp,
		ClampToBorder
	);

	ENUM(FilterMode, char,
		Linear,
		Nearest,
		Cubic
	);

	ENUM(InputChannels, char
		, RED
		, RG
		, RGB
		, RGBA
		, DEPTH_COMPONENT
	);


	ENUM(TextureTarget, int,
		PosX, NegX,
		PosY, NegY,
		PosZ, NegZ
	);

	ENUM(CMColorFormat, char,
		R_8,
		R_16,
		R_32F,
		R_64F,
		Rint_8,
		Rint_16,
		Rint_32,
		Rint_64,
		RG_8,
		RGF_16,
		RGB_8,
		RGBA_8,
		RGBF_16,
		RGBF_32,
		RGBAF_16,
		RGBAF_32,
		BGRA_8,
		RUI_32,
		DEPTH_COMPONENT,
		DXT1,
		DXT3,
		DXT5,
		DXT1_A,
		SRGB,
		SRGBA,
		SRGB_DXT1,
		SRGB_DXT3,
		SRGB_DXT5,
		SRGBA_DXT1
	);
	

	ENUM(CMUVMode, char,
		Repeat,
		MirrorRepeat,
		Clamp,
		ClampToBorder
	);

	ENUM(CMInputChannels, char
		, RED
		, RG
		, RGB
		, RGBA
	);


	ENUM(FontDefault, int,
		SourceSansPro
	); // 

	ENUM(FontColorFormat, char,
		R_8,
		R_16,
		R_32F,
		R_64F,
		Rint_8,
		Rint_16,
		Rint_32,
		Rint_64,
		RG_8,
		RGF_16,
		RGB_8,
		RGBA_8,
		RGBF_16,
		RGBF_32,
		RGBAF_16,
		RGBAF_32,
		BGRA_8,
		RUI_32,
		DEPTH_COMPONENT,
		DXT1,
		DXT3,
		DXT5,
		DXT1_A,
		SRGB,
		SRGBA,
		SRGB_DXT1,
		SRGB_DXT3,
		SRGB_DXT5,
		SRGBA_DXT1
	);

	ENUM(FontFilterMode, char,
		Linear,
		Nearest
	);

	ENUM(FontInputChannels, char
		, RED
		, RG
		, RGB
		, RGBA
		, DEPTH_COMPONENT
	);

	ENUM(FontUVMode, char,
		Repeat,
		MirrorRepeat,
		Clamp,
		ClampToBorder
	);

}