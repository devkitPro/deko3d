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

	w << CmdInline(2D, Operation{}, E2D::Operation::SrcCopy);
	w << CmdInline(2D, ClipEnable{}, 0);
	w << CmdInline(2D, Unknown221{}, 0x3f);
}

void dk::detail::BlitCopyEngine(DkCmdBuf obj, ImageInfo const& src, ImageInfo const& dst, uint32_t srcX, uint32_t srcY, uint32_t dstX, uint32_t dstY, uint32_t width, uint32_t height)
{
	CmdBufWriter w{obj};
	w.reserve(2*7 + 4 + 10);

	DkGpuAddr srcIova = src.m_iova;
	DkGpuAddr dstIova = dst.m_iova;
	uint32_t copyFlags = Copy::Execute::CopyMode{2} | Copy::Execute::Flush{} | Copy::Execute::Enable2D{};
	bool useSwizzle = false;

	if (src.m_isLinear)
		srcIova += srcY * src.m_horizontal + srcX * src.m_bytesPerBlock;
	else if (src.m_widthMs * src.m_bytesPerBlock > 0x10000)
		useSwizzle = true;

	if (dst.m_isLinear)
		dstIova += dstY * dst.m_horizontal + dstX * dst.m_bytesPerBlock;
	else if (dst.m_widthMs * dst.m_bytesPerBlock > 0x10000)
		useSwizzle = true;

	uint32_t srcHorizFactor = 1;
	uint32_t dstHorizFactor = 1;

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
			Copy::SrcPosXY::X{srcX} | Copy::SrcPosXY::Y{srcY});
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
			Copy::DstPosXY::X{dstX} | Copy::DstPosXY::Y{dstY});
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
	w << Cmd(Copy, XCount{}, width, height);
	w << Cmd(Copy, Execute{}, copyFlags);
}

void dk::detail::Blit2DEngine(DkCmdBuf obj, ImageInfo const& src, ImageInfo const& dst, uint32_t srcX, uint32_t srcY, uint32_t dstX, uint32_t dstY, uint32_t dstW, uint32_t dstH, uint32_t dudx, uint32_t dvdy, uint32_t fractBits, uint32_t flags)
{
	// TODO
}

void dkCmdBufPushData(DkCmdBuf obj, DkGpuAddr addr, const void* data, uint32_t size)
{
	obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_NotImplemented);
}

void dkCmdBufCopyBuffer(DkCmdBuf obj, DkGpuAddr srcAddr, DkGpuAddr dstAddr, uint32_t size)
{
	obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_NotImplemented);
}
