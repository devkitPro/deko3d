#include "../dk_device.h"
#include "../dk_queue.h"
#include "../dk_image.h"
#include "../cmdbuf_writer.h"

#include "engine_3d.h"
#include "engine_2d.h"
#include "engine_copy.h"

using namespace maxwell;
using namespace dk::detail;

using E3D  = Engine3D;
using E2D  = Engine2D;
using Copy = EngineCopy;

void tag_DkQueue::setupTransfer()
{
	CmdBufWriter w{&m_cmdBuf};

	w << Cmd(2D, BlendAlphaFactor{}, E2D::BlendAlphaFactor::Value{0xFF});
	w << CmdInline(2D, ClipEnable{}, 0);
	w << CmdInline(2D, Unknown221{}, 0x3f);
}

void dk::detail::BlitCopyEngine(DkCmdBuf obj, ImageInfo const& src, ImageInfo const& dst, BlitParams const& params)
{
	CmdBufWriter w{obj};
	w.reserve(2*7 + 4 + 10);

	DkGpuAddr srcIova = src.m_iova;
	DkGpuAddr dstIova = dst.m_iova;
	uint32_t copyFlags = Copy::Execute::CopyMode{2} | Copy::Execute::Flush{} | Copy::Execute::Enable2D{};
	bool useSwizzle = false;

	if (src.m_isLinear)
		srcIova += params.srcY * src.m_horizontal + params.srcX * src.m_bytesPerBlock;
	else if (src.m_widthMs * src.m_bytesPerBlock > 0x10000)
		useSwizzle = true;

	if (dst.m_isLinear)
		dstIova += params.dstY * dst.m_horizontal + params.dstX * dst.m_bytesPerBlock;
	else if (dst.m_widthMs * dst.m_bytesPerBlock > 0x10000)
		useSwizzle = true;

	uint32_t srcHorizFactor = 1;
	uint32_t dstHorizFactor = 1;
	uint32_t srcX = params.srcX;
	uint32_t dstX = params.dstX;
	uint32_t width = params.width;

	if (!useSwizzle)
	{
		srcX *= src.m_bytesPerBlock;
		dstX *= dst.m_bytesPerBlock;
		width *= src.m_bytesPerBlock;
		srcHorizFactor = src.m_bytesPerBlock;
		dstHorizFactor = dst.m_bytesPerBlock;
	}

	if (!src.m_isLinear)
	{
		w << Cmd(Copy, SrcTileMode{},
			src.m_tileMode | 0x1000,
			src.m_horizontal*srcHorizFactor,
			src.m_vertical,
			1U << ((src.m_tileMode >> 8) & 0xF),
			0,
			Copy::SrcPosXY::X{srcX} | Copy::SrcPosXY::Y{params.srcY});
	}
	else
	{
		copyFlags |= Copy::Execute::SrcIsPitchLinear{};
		w << Cmd(Copy, SrcPitch{}, src.m_horizontal);
	}

	if (!dst.m_isLinear)
	{
		w << Cmd(Copy, DstTileMode{},
			dst.m_tileMode | 0x1000,
			dst.m_horizontal*dstHorizFactor,
			dst.m_vertical,
			1U << ((dst.m_tileMode >> 8) & 0xF),
			0,
			Copy::DstPosXY::X{dstX} | Copy::DstPosXY::Y{params.dstY});
	}
	else
	{
		copyFlags |= Copy::Execute::DstIsPitchLinear{};
		w << Cmd(Copy, DstPitch{}, dst.m_horizontal);
	}

	if (useSwizzle)
	{
		copyFlags |= Copy::Execute::EnableSwizzle{};
		uint32_t compSize = 2;
		if (src.m_bytesPerBlock == 4 || src.m_bytesPerBlock == 8 || src.m_bytesPerBlock == 16)
			compSize = 4;

		using S = Copy::Swizzle;
		w << Cmd(Copy, Const{}, 0, 0,
			S::X{0} | S::Y{1} | S::Z{2} | S::W{3} |
			S::SizeMinus1{compSize-1} |
			S::SrcNumComponentsMinus1{src.m_bytesPerBlock/compSize-1} |
			S::DstNumComponentsMinus1{dst.m_bytesPerBlock/compSize-1});
	}

	w << Cmd(Copy, SrcAddr{}, Iova(srcIova), Iova(dstIova));
	w << Cmd(Copy, XCount{}, width, params.height);
	w << Cmd(Copy, Execute{}, copyFlags);
}

void dk::detail::Blit2DEngine(DkCmdBuf obj, ImageInfo const& src, ImageInfo const& dst, BlitParams const& params, int32_t dudx, int32_t dvdy, uint32_t flags, uint32_t factor)
{
	CmdBufWriter w{obj};
	w.reserve((3 + 10*2 + 1) + 14);

	if (flags & Blit2D_SetupEngine)
	{
		using BO = E2D::Operation;
		uint32_t blitOp;
		bool hasFactor = false;
		switch (flags & DkBlitFlag_Mode_Mask)
		{
			default:
			case DkBlitFlag_ModeBlit:
				blitOp = BO::SrcCopy;
				break;
			case DkBlitFlag_ModeAlphaMask:
				blitOp = BO::SrcCopyAnd;
				break;
			case DkBlitFlag_ModeAlphaBlend:
				blitOp = BO::Blend;
				break;
			case DkBlitFlag_ModePremultBlit:
				blitOp = BO::SrcCopyPremult;
				hasFactor = true;
				break;
			case DkBlitFlag_ModePremultBlend:
				blitOp = BO::BlendPremult;
				hasFactor = true;
				break;
		}

		w << CmdInline(2D, Operation{}, blitOp);
		if (hasFactor)
			w << Cmd(2D, BlendPremultFactor{}, factor);

		if (!src.m_isLinear)
		{
			w << Cmd(2D, SrcFormat{}, src.m_format, 0, src.m_tileMode, src.m_arrayMode);
			w << Cmd(2D, SrcHorizontal{}, src.m_horizontal, src.m_vertical, Iova(src.m_iova));
		}
		else
		{
			w << Cmd(2D, SrcFormat{}, src.m_format, 1);
			w << Cmd(2D, SrcPitch{}, src.m_horizontal, src.m_width, src.m_height, Iova(src.m_iova));
		}

		if (!dst.m_isLinear)
		{
			w << Cmd(2D, DestFormat{}, dst.m_format, 0, dst.m_tileMode, dst.m_arrayMode);
			w << Cmd(2D, DestHorizontal{}, dst.m_horizontal, dst.m_vertical, Iova(dst.m_iova));
		}
		else
		{
			w << Cmd(2D, DestFormat{}, dst.m_format, 1);
			w << Cmd(2D, DestPitch{}, dst.m_horizontal, dst.m_width, dst.m_height, Iova(dst.m_iova));
		}

		w << CmdInline(2D, Unknown0b5{}, 1);
	}
	else
	{
		w << Cmd(2D, SrcAddr{}, Iova(src.m_iova));
		w << Cmd(2D, DestAddr{}, Iova(dst.m_iova));
	}

	uint32_t blitCtrlFlags = 0;
	using BC = E2D::BlitControl;
	if (flags & Blit2D_OriginCorner)
		blitCtrlFlags |= BC::Origin::Corner;
	if (flags & Blit2D_UseFilter)
		blitCtrlFlags |= BC::Filter::Linear;

	w << CmdInline(2D, BlitControl{}, blitCtrlFlags);
	w << Cmd(2D, BlitDestX{},
		params.dstX,
		params.dstY,
		params.width,
		params.height,
		dudx << (32 - DiffFractBits),
		dudx >> DiffFractBits,
		dvdy << (32 - DiffFractBits),
		dvdy >> DiffFractBits,
		params.srcX << (32 - SrcFractBits),
		params.srcX >> SrcFractBits,
		params.srcY << (32 - SrcFractBits),
		params.srcY >> SrcFractBits
	);
}

void dkCmdBufPushData(DkCmdBuf obj, DkGpuAddr addr, const void* data, uint32_t size)
{
	obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_NotImplemented);
}

void dkCmdBufCopyBuffer(DkCmdBuf obj, DkGpuAddr srcAddr, DkGpuAddr dstAddr, uint32_t size)
{
	obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_NotImplemented);
}
