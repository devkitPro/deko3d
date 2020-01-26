#include <math.h>
#include "../dk_device.h"
#include "../dk_queue.h"
#include "../dk_image.h"
#include "../cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"

#include "../driver_constbuf.h"
#include "texture_image_control_block.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;
using SRC = E::MmeShadowRamControl;

namespace
{
	constexpr auto SetShadowRamControl(unsigned mode)
	{
		return CmdInline(3D, MmeShadowRamControl{}, mode);
	}

	template <typename T>
	constexpr auto MacroFillRegisters(unsigned base, unsigned increment, unsigned count, T value)
	{
		return Macro(FillRegisters, base | (increment << 12), count, value);
	}

	template <typename Array, typename T>
	constexpr auto MacroFillArray(T value)
	{
		return MacroFillRegisters(Array{}, 1U, Array::Size, value);
	}

	template <typename Array, typename T>
	constexpr auto MacroFillRegisterArray(T value)
	{
		return MacroFillRegisters(Array{}, 1U, Array::Count << Array::Shift, value);
	}

	template <typename Array, typename T>
	constexpr auto MacroSetRegisterInArray(unsigned offset, T value)
	{
		return MacroFillRegisters(Array{} + offset, 1U << Array::Shift, Array::Count, value);
	}

	constexpr auto ColorTargetBindCmds(ImageInfo& info, unsigned id)
	{
		return Cmd(3D, RenderTarget::Addr{id},
			Iova(info.m_iova),
			info.m_horizontal, info.m_vertical,
			info.m_format, info.m_tileMode, info.m_arrayMode, info.m_layerStride);
	}
}

void Queue::setup3DEngine()
{
	CmdBufWriter w{&m_cmdBuf};

	w << CmdInline(3D, MultisampleEnable{}, 0);
	w << CmdInline(3D, CsaaEnable{}, 0);
	w << CmdInline(3D, MultisampleMode{}, MsaaMode_1x1);
	w << CmdInline(3D, MultisampleControl{}, 0);
	w << CmdInline(3D, Unknown1d3{}, 0x3F);
	w << MacroFillRegisterArray<E::WindowRectangle>(0);
	w << CmdInline(3D, ClearBufferFlags{}, E::ClearBufferFlags::StencilMask{} | E::ClearBufferFlags::Scissor{});
	w << MacroSetRegisterInArray<E::Scissor>(E::Scissor::Enable{}, 1);
	w << Cmd(3D, Unknown5aa{}, 0x00030003);
	w << Cmd(3D, Unknown5e5{}, 0x00020002);
	w << CmdInline(3D, PrimitiveRestartWithDrawArrays{}, 0);
	w << CmdInline(3D, PointRasterRules{}, 0);
	w << CmdInline(3D, LinkedTsc{}, 0);
	w << CmdInline(3D, ProvokingVertexLast{}, 1);
	w << CmdInline(3D, SetShaderExceptions{}, 0);
	w << CmdInline(3D, Unknown400{}, 0x10);
	w << CmdInline(3D, Unknown086{}, 0x10);
	w << CmdInline(3D, Unknown43f{}, 0x10);
	w << CmdInline(3D, Unknown4a4{}, 0x10);
	w << CmdInline(3D, Unknown4b6{}+0, 0x10);
	w << CmdInline(3D, Unknown4b6{}+1, 0x10);
	w << CmdInline(3D, SetApiVisibleCallLimit{}, E::SetApiVisibleCallLimit::Cta::_128);
	w << CmdInline(3D, Unknown450{}, 0x10);
	w << CmdInline(3D, Unknown584{}, 0x0E);
	w << MacroFillArray<E::IsVertexArrayPerInstance>(0);
	w << CmdInline(3D, VertexIdConfig{}, E::VertexIdConfig::DrawArraysAddStart{});
	w << CmdInline(3D, ZcullStatCountersEnable{}, 1);
	w << CmdInline(3D, LineWidthSeparate{}, 1);
	w << CmdInline(3D, Unknown0c3{}, 0);
	w << CmdInline(3D, Unknown0c0{}, 3);
	w << CmdInline(3D, Unknown3f7{}, 1);
	w << CmdInline(3D, Unknown670{}, 1);
	w << CmdInline(3D, Unknown3e3{}, 0);
	w << CmdInline(3D, StencilTwoSideEnable{}, 1);
	w << CmdInline(3D, SetBindlessTexture{}, 0); // Using constbuf0 as the texture constbuf
	w << CmdInline(3D, SetSpaVersion{},
		E::SetSpaVersion::Major{5} | E::SetSpaVersion::Minor{3} // SM 5.3
	);
	w << Cmd(3D, SetShaderLocalMemoryWindow{}, 0x01000000);
	w << Cmd(3D, SetShaderLocalMemory{}, Iova(m_workBuf.getScratchMem()), Iova(m_workBuf.getScratchMemSize()));
	w << CmdInline(3D, Unknown44c{}, 0x13);
	w << CmdInline(3D, Unknown0dd{}, 0x00);
	w << Cmd(3D, SetRenderLayer{}, E::SetRenderLayer::UseIndexFromVTG{});
	w << CmdInline(3D, Unknown488{}, 5);
	w << Cmd(3D, Unknown514{}, 8 | (getDevice()->getGpuInfo().numWarpsPerSm << 16));
	// here there would be 2D engine commands
	w << Macro(WriteHardwareReg, 0x00418800, 0x00000001, 0x00000001);
	w << Macro(WriteHardwareReg, 0x00419A08, 0x00000000, 0x00000010);
	w << Macro(WriteHardwareReg, 0x00419F78, 0x00000000, 0x00000008);
	w << Macro(WriteHardwareReg, 0x00404468, 0x07FFFFFF, 0x3FFFFFFF);
	w << Macro(WriteHardwareReg, 0x00419A04, 0x00000001, 0x00000001);
	w << Macro(WriteHardwareReg, 0x00419A04, 0x00000002, 0x00000002);
	w << CmdInline(3D, ZcullUnknown65a{}, 0x11);
	w << CmdInline(3D, ZcullTestMask{}, 0x00);
	w << CmdInline(3D, ZcullRegion{}, hasZcull() ? 0 : 0x3f);
	w << Cmd(3D, SetInstrumentationMethodHeader{}, 0x49000000);
	w << Cmd(3D, SetInstrumentationMethodData{}, 0x49000001);
	w << Cmd(3D, MmeDriverConstbufIova{}, m_workBuf.getGraphicsCbuf() >> 8, m_workBuf.getGraphicsCbufSize());
	w << Cmd(3D, VertexRunoutBufferIova{}, Iova(m_workBuf.getVtxRunoutBuf()));
	w << Macro(SelectDriverConstbuf, 0);
	w << MacroSetRegisterInArray<E::Bind>(E::Bind::Constbuf{},
		E::Bind::Constbuf::Valid{} | E::Bind::Constbuf::Index{0}
	);

	w << CmdInline(3D, AdvancedBlendEnable{}, 0);
	w << CmdInline(3D, IndependentBlendEnable{}, 1);
	w << CmdInline(3D, EdgeFlag{}, 1);
	w << CmdInline(3D, ViewportTransformEnable{}, 1);
	using VVCC = E::ViewVolumeClipControl;
	w << Cmd(3D, ViewVolumeClipControl{},
		VVCC::ForceDepthRangeZeroToOne{} | VVCC::Unknown1{2} |
		VVCC::DepthClampNear{} | VVCC::DepthClampFar{} |
		VVCC::Unknown11{} | VVCC::Unknown12{1}
	);

	// For MinusOneToOne mode, we need to do this transform: newZ = 0.5*oldZ + 0.5
	// For ZeroToOne mode, the incoming depth value is already in the correct range.
	bool isDepthOpenGL = getDevice()->isDepthModeOpenGL();
	w << CmdInline(3D, SetDepthMode{}, isDepthOpenGL ? E::SetDepthMode::MinusOneToOne : E::SetDepthMode::ZeroToOne); // this controls pre-transform clipping
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::ScaleZ{}, isDepthOpenGL ? 0.5f : 1.0f);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::TranslateZ{}, isDepthOpenGL ? 0.5f : 0.0f);

	// Configure viewport transform XY to convert [-1,1] into [0,1]: newXY = 0.5*oldXY + 0.5
	// Additionally, for UpperLeft origin mode, the Y value needs to be reversed since the incoming Y coordinate points up,
	// and that needs to be fixed to point down.
	// Also, SetWindowOriginMode seems to affect how the hardware treats polygons as front-facing or back-facing,
	// but it doesn't actually flip rendering.
	bool isOriginOpenGL = getDevice()->isOriginModeOpenGL();
	w << CmdInline(3D, SetWindowOriginMode{}, isOriginOpenGL ? E::SetWindowOriginMode::Mode::LowerLeft : E::SetWindowOriginMode::Mode::UpperLeft);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::ScaleX{}, +0.5f);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::ScaleY{}, isOriginOpenGL ? +0.5f : -0.5f);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::TranslateX{}, +0.5f);
	w << MacroSetRegisterInArray<E::ViewportTransform>(E::ViewportTransform::TranslateY{}, +0.5f);
	w << MacroSetRegisterInArray<E::Viewport>(E::Viewport::Horizontal{}, 0U | (1U << 16)); // x=0 w=1
	w << MacroSetRegisterInArray<E::Viewport>(E::Viewport::Vertical{},   0U | (1U << 16)); // y=0 h=1

	w << SetShadowRamControl(SRC::MethodTrack);
	w << MacroSetRegisterInArray<E::Scissor>(E::Scissor::Horizontal{}, 0xFFFF0000);
	w << MacroSetRegisterInArray<E::Scissor>(E::Scissor::Vertical{},   0xFFFF0000);
	w << SetShadowRamControl(SRC::MethodTrackWithFilter);

	w << MacroFillArray<E::MmeProgramIds>(0);
	w << MacroFillArray<E::MmeProgramOffsets>(0);
	w << Cmd(3D, MmeDepthRenderTargetIova{}, 0xFFFFFFFF);

	w << Cmd(3D, IndexArrayLimitIova{}, Iova(0xFFFFFFFFFFUL));
	w << CmdInline(3D, SampleCounterEnable{}, 1);
	w << CmdInline(3D, ClipDistanceEnable{}, 0xFF); // Enable all clip distances
	w << MacroFillArray<E::MsaaMask>(0xFFFF);
	w << CmdInline(3D, ColorReductionEnable{}, 0);
	w << CmdInline(3D, PointSpriteEnable{}, 1);
	w << CmdInline(3D, PointCoordReplace{}, E::PointCoordReplace::Enable{1});
	w << CmdInline(3D, VertexProgramPointSize{}, E::VertexProgramPointSize::Enable{});
	w << CmdInline(3D, PipeNop{}, 0);

	w << CmdInline(3D, StencilFrontFuncMask{}, 0xFF);
	w << CmdInline(3D, StencilFrontMask{}, 0xFF);
	w << CmdInline(3D, StencilBackFuncMask{}, 0xFF);
	w << CmdInline(3D, StencilBackMask{}, 0xFF);

	w << CmdInline(3D, DepthTargetArrayMode{}, E::DepthTargetArrayMode::Layers{1});
	w << CmdInline(3D, SetConservativeRasterEnable{}, 0);
	w << Macro(WriteHardwareReg, 0x00418800, 0x00000000, 0x01800000);
	w << CmdInline(3D, Unknown0bb{}, 0);

	w << CmdInline(3D, SetMultisampleRasterEnable{}, 0);
	w << CmdInline(3D, SetCoverageModulationTableEnable{}, 0);
	w << CmdInline(3D, Unknown44c{}, 0x13);
	w << CmdInline(3D, SetMultisampleCoverageToColorEnable{}, 0);

	w << Cmd(3D, SetProgramRegion{}, Iova(getDevice()->getCodeSeg().getBase()));
	w << CmdInline(3D, Unknown5ad{}, 0);

	// The following pgraph register writes apparently initialize the tile cache hardware.
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00000007, 0x0000000F);
	w << Macro(WriteHardwareReg, 0x00418E58, 0x00000842, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00000070, 0x000000F0);
	w << Macro(WriteHardwareReg, 0x00418E58, 0x04F10000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00000700, 0x00000F00);
	w << Macro(WriteHardwareReg, 0x00418E5C, 0x00000053, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00007000, 0x0000F000);
	w << Macro(WriteHardwareReg, 0x00418E5C, 0x00E90000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00070000, 0x000F0000);
	w << Macro(WriteHardwareReg, 0x00418E60, 0x000000EA, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x00700000, 0x00F00000);
	w << Macro(WriteHardwareReg, 0x00418E60, 0x00EB0000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x07000000, 0x0F000000);
	w << Macro(WriteHardwareReg, 0x00418E64, 0x00000208, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E40, 0x70000000, 0xF0000000);
	w << Macro(WriteHardwareReg, 0x00418E64, 0x02090000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E44, 0x00000007, 0x0000000F);
	w << Macro(WriteHardwareReg, 0x00418E68, 0x0000020A, 0x0000FFFF);
	w << Macro(WriteHardwareReg, 0x00418E44, 0x00000070, 0x000000F0);
	w << Macro(WriteHardwareReg, 0x00418E68, 0x020B0000, 0xFFFF0000);
	w << Macro(WriteHardwareReg, 0x00418E44, 0x00000700, 0x00000F00);
	w << Macro(WriteHardwareReg, 0x00418E6C, 0x00000644, 0x0000FFFF);

	w << Cmd(3D, TiledCacheTileSize{}, 0x80 | (0x80 << 16));
	w << Cmd(3D, TiledCacheUnknownConfig0{}, 0x00001109);
	w << Cmd(3D, TiledCacheUnknownConfig1{}, 0x08080202);
	w << Cmd(3D, TiledCacheUnknownConfig2{}, 0x0000001F);
	w << Cmd(3D, TiledCacheUnknownConfig3{}, 0x00080001);
	w << CmdInline(3D, TiledCacheUnkFeatureEnable{}, 0);
}

void dkCmdBufBindRenderTargets(DkCmdBuf obj, DkImageView const* const colorTargets[], uint32_t numColorTargets, DkImageView const* depthTarget)
{
#ifdef DEBUG
	if (numColorTargets > DK_MAX_RENDER_TARGETS)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	CmdBufWriter w{obj};
	w.reserve(2 + numColorTargets*9 + (8-numColorTargets)*1 + (depthTarget ? (12+14) : 1) + 4);

	w << Cmd(3D, RenderTargetControl{},
		E::RenderTargetControl::NumTargets{numColorTargets} | (076543210<<4)
	);

	uint32_t minWidth = 0xFFFF, minHeight = 0xFFFF;
	DkMsMode msMode = DkMsMode_1x;
	for (uint32_t i = 0; i < numColorTargets; i ++)
	{
		auto* view = colorTargets[i];
#ifdef DEBUG
		if (!view || !view->pImage)
			obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		if (!(view->pImage->m_flags & DkImageFlags_UsageRender))
			obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif
		ImageInfo rt;
#ifdef DEBUG
		DkResult res = rt.fromImageView(view, ImageInfo::ColorRenderTarget);
		if (res != DkResult_Success)
		{
			obj->raiseError(DK_FUNC_ERROR_CONTEXT, res);
			return;
		}
#else
		rt.fromImageView(view, ImageInfo::ColorRenderTarget);
#endif
		w << ColorTargetBindCmds(rt, i);

		// Update data
		DkImage const* image = view->pImage;
		if (image->m_dimensions[0] < minWidth)  minWidth  = image->m_dimensions[0];
		if (image->m_dimensions[1] < minHeight) minHeight = image->m_dimensions[1];
		msMode = (DkMsMode)image->m_numSamplesLog2;
	}

	// Disable all remaining render targets
	for (uint32_t i = numColorTargets; i < DK_MAX_RENDER_TARGETS; i ++)
	{
		// Writing format 0 seems to disable this render target?
		w << CmdInline(3D, RenderTarget::Format{i}, 0);
	}

	if (!depthTarget)
	{
		w << CmdInline(3D, DepthTargetEnable{}, 0);
		// mme scratch "weird zcull feature enable" is set to zero here
	}
	else
	{
		ImageInfo rt;
#ifdef DEBUG
		DkResult res = rt.fromImageView(depthTarget, ImageInfo::DepthRenderTarget);
		if (res != DkResult_Success)
		{
			obj->raiseError(DK_FUNC_ERROR_CONTEXT, res);
			return;
		}
#else
		rt.fromImageView(depthTarget, ImageInfo::DepthRenderTarget);
#endif
		w << Cmd(3D, DepthTargetAddr{}, Iova(rt.m_iova), rt.m_format, rt.m_tileMode, rt.m_layerStride);
		w << CmdInline(3D, DepthTargetEnable{}, 0);
		w << CmdInline(3D, DepthTargetEnable{}, 1);
		w << Cmd(3D, DepthTargetHorizontal{}, rt.m_horizontal, rt.m_vertical, rt.m_arrayMode);

		DkImage const* image = depthTarget->pImage;
		ZcullStorageInfo zinfo;
		obj->getDevice()->calcZcullStorageInfo(zinfo,
			image->m_dimensions[0], image->m_dimensions[1], rt.m_arrayMode,
			depthTarget->format ? depthTarget->format : image->m_format,
			(DkMsMode)image->m_bytesPerBlockLog2);

		// Configure Zcull
		w << Cmd(3D, ZcullImageSizeAliquots{}, zinfo.imageSize<<16, zinfo.layerSize);
		w << CmdInline(3D, ZcullZetaType{}, zinfo.zetaType);
		w << Cmd(3D, ZcullWidth{}, zinfo.width, zinfo.height, zinfo.depth);
		w << CmdInline(3D, ZcullWindowOffsetX{}, 0);
		w << CmdInline(3D, ZcullWindowOffsetY{}, 0);
		w << CmdInline(3D, ZcullUnknown0{}, 0);
		w << CmdInline(3D, ZcullUnkFeatureEnable{}, 0);
		w << Macro(ConditionalZcullInvalidate, rt.m_iova >> 8);
		// mme scratch "weird zcull feature enable" is set here

		// Update data
		if (image->m_dimensions[0] < minWidth)  minWidth  = image->m_dimensions[0];
		if (image->m_dimensions[1] < minHeight) minHeight = image->m_dimensions[1];
		if (!numColorTargets) msMode = (DkMsMode)image->m_numSamplesLog2;
	}

	// Configure screen scissor
	if (minWidth  > 0x4000) minWidth  = 0x4000;
	if (minHeight > 0x4000) minHeight = 0x4000;
	w << Cmd(3D, ScreenScissorHorizontal{}, minWidth<<16, minHeight<<16);

	// Configure msaa mode
	MsaaMode msaaMode;
	switch (msMode)
	{
		default:
		case DkMsMode_1x:
			msaaMode = MsaaMode_1x1;
			break;
		case DkMsMode_2x:
			msaaMode = MsaaMode_2x1_D3D;
			break;
		case DkMsMode_4x:
			msaaMode = MsaaMode_2x2;
			break;
		case DkMsMode_8x:
			msaaMode = MsaaMode_4x2_D3D;
			break;
		/* not yet
		case DkMsMode_16x:
			msaaMode = MsaaMode_4x4;
			break;
		*/
	}
	w << CmdInline(3D, MultisampleMode{}, msaaMode);
}

void dkCmdBufSetViewports(DkCmdBuf obj, uint32_t firstId, DkViewport const viewports[], uint32_t numViewports)
{
#ifdef DEBUG
	if (firstId > DK_NUM_VIEWPORTS || numViewports > DK_NUM_VIEWPORTS || (firstId+numViewports) > DK_NUM_VIEWPORTS)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (numViewports && !viewports)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	bool isOriginOpenGL = obj->getDevice()->isOriginModeOpenGL();
	bool isDepthOpenGL  = obj->getDevice()->isDepthModeOpenGL();

	CmdBufWriter w{obj};
	w.reserve(12*numViewports);

	for (uint32_t i = 0; i < numViewports; i ++)
	{
		DkViewport const& v = viewports[i];
		uint32_t id = firstId + i;

		float halfWidth = 0.5f*v.width;
		float halfHeight = 0.5f*v.height;
		float depthRange = v.far - v.near;

		float scaleX = halfWidth;
		float scaleY = isOriginOpenGL ? halfHeight : (-halfHeight);
		float scaleZ = isDepthOpenGL ? 0.5f*depthRange : depthRange;

		float offsetX = v.x + halfWidth;
		float offsetY = v.y + halfHeight;
		float offsetZ = isDepthOpenGL ? 0.5f*(v.near + v.far) : v.near;

		uint32_t viewportX = floorf(v.x);
		uint32_t viewportY = floorf(v.y);
		uint32_t viewportW = ceilf(v.x+v.width)-viewportX;
		uint32_t viewportH = ceilf(v.y+v.height)-viewportY;

		float viewportNear = depthRange >= 0 ? v.near : v.far;
		float viewportFar  = depthRange >= 0 ? v.far  : v.near;

		w << Cmd(3D, ViewportTransform::ScaleX{id},
			scaleX, scaleY, scaleZ,
			offsetX, offsetY, offsetZ);
		w << Cmd(3D, Viewport::Horizontal{id},
			viewportX | (viewportW << 16),
			viewportY | (viewportH << 16),
			viewportNear,
			viewportFar);
	}
}

void dkCmdBufSetViewportSwizzles(DkCmdBuf obj, uint32_t firstId, DkViewportSwizzle const swizzles[], uint32_t numSwizzles)
{
#ifdef DEBUG
	if (firstId > DK_NUM_VIEWPORTS || numSwizzles > DK_NUM_VIEWPORTS || (firstId+numSwizzles) > DK_NUM_VIEWPORTS)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (numSwizzles && !swizzles)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	CmdBufWriter w{obj};
	w.reserve(2*numSwizzles);

	for (uint32_t i = 0; i < numSwizzles; i ++)
	{
		DkViewportSwizzle const& sw = swizzles[i];
		uint32_t id = firstId + i;

		using S = Engine3D::ViewportTransform::Swizzles;
		w << Cmd(3D, ViewportTransform::Swizzles{id},
			S::X{sw.x} | S::Y{sw.y} | S::Z{sw.z} | S::W{sw.w});
	}
}

void dkCmdBufSetScissors(DkCmdBuf obj, uint32_t firstId, DkScissor const scissors[], uint32_t numScissors)
{
#ifdef DEBUG
	if (firstId > DK_NUM_SCISSORS || numScissors > DK_NUM_SCISSORS || (firstId+numScissors) > DK_NUM_SCISSORS)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (numScissors && !scissors)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	CmdBufWriter w{obj};
	w.reserve(3*numScissors);

	for (uint32_t i = 0; i < numScissors; i ++)
	{
		DkScissor const& s = scissors[i];
		uint32_t id = firstId + i;

		w << Cmd(3D, Scissor::Horizontal{id},
			s.x | ((s.x+s.width) <<16),
			s.y | ((s.y+s.height)<<16)
		);
	}

}

void Queue::decompressSurface(DkImage const* image)
{
	ImageInfo rt = {};
	DkImageView view;
	dkImageViewDefaults(&view, image);
	rt.fromImageView(&view, ImageInfo::ColorRenderTarget);

	CmdBufWriter w{&m_cmdBuf};
	w.reserve(34);

	w << SetShadowRamControl(SRC::MethodPassthrough);
	w << ColorTargetBindCmds(rt, 0);
	w << CmdInline(3D, MultisampleMode{}, DkMsMode_1x);
	w << Cmd(3D, ScreenScissorHorizontal{}, rt.m_width<<16, rt.m_height<<16);
	w << CmdInline(3D, SetWindowOriginMode{}, E::SetWindowOriginMode::Mode::LowerLeft); // ??
	w << CmdInline(3D, SetMultisampleRasterEnable{}, 0);

	w << CmdInline(3D, SurfaceDecompress{}, 0);

	w << SetShadowRamControl(SRC::MethodReplay);
	w << ColorTargetBindCmds(rt, 0);
	w << CmdInline(3D, MultisampleMode{}, DkMsMode_1x);
	w << Cmd(3D, ScreenScissorHorizontal{}, rt.m_width<<16, rt.m_height<<16);
	w << CmdInline(3D, SetWindowOriginMode{}, E::SetWindowOriginMode::Mode::LowerLeft); // ??
	w << CmdInline(3D, SetMultisampleRasterEnable{}, 0);

	w << SetShadowRamControl(SRC::MethodTrackWithFilter);
}

void dkCmdBufClearColor(DkCmdBuf obj, uint32_t targetId, uint32_t clearMask, const void* clearData)
{
#ifdef DEBUG
	if (targetId >= DK_MAX_RENDER_TARGETS)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!clearData)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif
	if (!clearMask)
		return;

	CmdBufWriter w{obj};
	w.reserve(12);

	w << CmdList<1>{ MakeCmdHeader(Increasing, 4, Subchannel3D, E::ClearColor{}) };
	w.addRawData(clearData, 4*sizeof(float));

	w << SetShadowRamControl(SRC::MethodPassthrough);
	w << CmdInline(3D, SetMultisampleRasterEnable{}, 0);
	w << Macro(ClearColor,
		((clearMask&0xF)<<E::ClearBuffers::Red::Shift) | E::ClearBuffers::TargetId{targetId}
	);

	w << SetShadowRamControl(SRC::MethodReplay);
	w << CmdInline(3D, SetMultisampleRasterEnable{}, 0);

	w << SetShadowRamControl(SRC::MethodTrackWithFilter);
}

void dkCmdBufClearDepthStencil(DkCmdBuf obj, bool clearDepth, float depthValue, uint8_t stencilMask, uint8_t stencilValue)
{
	if (!clearDepth || !stencilMask)
		return;

	CmdBufWriter w{obj};
	w.reserve(13);

	using CB = E::ClearBuffers;
	uint32_t clearArg = 0;

	if (clearDepth)
	{
		using ZCD = E::ZcullClearDepth;
		bool isDepthLessThanHalf = depthValue<0.5f;
		bool isDepthOneOrZero    = depthValue==1.0f || depthValue==0.0f;
		w << Cmd(3D, ZcullClearDepth{}, ZCD::IsLessThanHalf{isDepthLessThanHalf} | ZCD::IsOneOrZero{isDepthOneOrZero});
		w << Cmd(3D, ZcullClearMode{}, 0xFF000005); // ??
		w << Cmd(3D, ClearDepth{}, depthValue);
		clearArg |= CB::Depth{};
	}

	if (stencilMask)
	{
		w << SetShadowRamControl(SRC::MethodPassthrough);
		w << CmdInline(3D, StencilFrontMask{}, stencilMask);
		w << CmdInline(3D, ClearStencil{}, stencilValue);
		clearArg |= CB::Stencil{};
	}

	w << MacroInline(ClearDepthStencil, clearArg);

	if (stencilMask)
	{
		w << SetShadowRamControl(SRC::MethodReplay);
		w << CmdInline(3D, StencilFrontMask{}, 0);
		w << SetShadowRamControl(SRC::MethodTrackWithFilter);
	}
}

void dkCmdBufDiscardColor(DkCmdBuf obj, uint32_t targetId)
{
	CmdBufWriter w{obj};
	w.reserve(1);

	w << CmdInline(3D, DiscardRenderTarget{}, E::DiscardRenderTarget::Color{targetId});
}

void dkCmdBufDiscardDepthStencil(DkCmdBuf obj)
{
	CmdBufWriter w{obj};
	w.reserve(1);

	w << CmdInline(3D, DiscardRenderTarget{}, E::DiscardRenderTarget::DepthStencil{});
}

void dkCmdBufDraw(DkCmdBuf obj, DkPrimitive prim, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	CmdBufWriter w{obj};
	w.reserve(7);

	if (firstInstance)
		w << MacroInline(SelectDriverConstbuf, 0); // needed for updating gl_BaseInstance in the driver constbuf
	w << Macro(Draw, prim, vertexCount, instanceCount, firstVertex, firstInstance);
}

void dkCmdBufDrawIndirect(DkCmdBuf obj, DkPrimitive prim, DkGpuAddr indirect)
{
#ifdef DEBUG
	if (indirect == DK_GPU_ADDR_INVALID)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (indirect & 3)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
#endif

	CmdBufWriter w{obj};
	w.reserve(3);

	w << MacroInline(SelectDriverConstbuf, 0); // needed for updating gl_BaseInstance in the driver constbuf
	w << CmdList<2>{ MakeCmdHeader(IncreaseOnce, 5, Subchannel3D, MmeMacroDraw), prim };
	w.split(CtrlCmdGpfifoEntry::NoPrefetch);
	w.addRaw(indirect, 4, CtrlCmdGpfifoEntry::AutoKick);
}

void dkCmdBufDrawIndexed(DkCmdBuf obj, DkPrimitive prim, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	CmdBufWriter w{obj};
	w.reserve(8);

	if (vertexOffset || firstInstance)
		w << MacroInline(SelectDriverConstbuf, 0); // needed for updating gl_BaseVertex/gl_BaseInstance in the driver constbuf
	w << Macro(DrawIndexed, prim, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void dkCmdBufDrawIndexedIndirect(DkCmdBuf obj, DkPrimitive prim, DkGpuAddr indirect)
{
#ifdef DEBUG
	if (indirect == DK_GPU_ADDR_INVALID)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (indirect & 3)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
#endif

	CmdBufWriter w{obj};
	w.reserve(3);

	w << MacroInline(SelectDriverConstbuf, 0); // needed for updating gl_BaseVertex/gl_BaseInstance in the driver constbuf
	w << CmdList<2>{ MakeCmdHeader(IncreaseOnce, 6, Subchannel3D, MmeMacroDrawIndexed), prim };
	w.split(CtrlCmdGpfifoEntry::NoPrefetch);
	w.addRaw(indirect, 5, CtrlCmdGpfifoEntry::AutoKick);
}
