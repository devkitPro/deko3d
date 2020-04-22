#include <math.h>
#include "../dk_device.h"
#include "../cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;
using SRC = E::MmeShadowRamControl;

namespace
{
	constexpr uint32_t getBlendFactorSetting(DkBlendFactor factor)
	{
		// Explanation:
		// Nvidia's color blend factor registers accept both D3D11_BLEND and GLenum values.
		// In order to use GLenum values, bit 14 (0x4000) needs to be set.
		// DkBlendFactor lines up with D3D11_BLEND in most of its values except for the
		// ones involving the constant blend color, for which GLenum values are used
		// (after a little bit of tweaking so that the whole thing fits in just 6 bits).
		uint32_t ret = factor & 0x1f;
		if (factor & 0x20)
			ret |= 0xc000;
		return ret;
	}
}

void dkCmdBufBindRasterizerState(DkCmdBuf obj, DkRasterizerState const* state)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(state);
	CmdBufWriter w{obj};
	w.reserve(14);

	w << CmdInline(3D, RasterizerEnable{}, state->rasterizerEnable);
	if (!state->rasterizerEnable)
		return;

	w << CmdInline(3D, ViewVolumeClipControl{}, state->depthClampEnable ? 0x101A : 0x181D); // todo: figure out what this actually does
	w << CmdInline(3D, FillRectangleConfig{}, E::FillRectangleConfig::Enable{state->fillRectangleEnable});

	DkPolygonMode front = state->polygonModeFront, back = state->polygonModeBack;
	if (state->fillRectangleEnable)
	{
		// NV_fill_rectangle makes no sense to enable if the polygon mode isn't Fill.
		front = DkPolygonMode_Fill;
		back = DkPolygonMode_Fill;
	}

	static const uint16_t polygonModes[] = { E::SetPolygonModeFront::Point, E::SetPolygonModeFront::Line, E::SetPolygonModeFront::Fill, 0 };
	w << CmdInline(3D, SetPolygonModeFront{}, polygonModes[front]);
	w << CmdInline(3D, SetPolygonModeBack{},  polygonModes[back]);

	w << CmdInline(3D, CullFaceEnable{}, state->cullMode != DkFace_None);
	if (state->cullMode != DkFace_None)
	{
		static const uint16_t cullModes[] = { E::SetCullFace::Front, E::SetCullFace::Back, E::SetCullFace::FrontAndBack };
		w << CmdInline(3D, SetCullFace{}, cullModes[state->cullMode-1]);
	}

	static const uint16_t frontFaces[] = { E::SetFrontFace::CW, E::SetFrontFace::CCW };
	w << CmdInline(3D, SetFrontFace{}, frontFaces[state->frontFace]);

	w << CmdInline(3D, PointSmoothEnable{},        (state->polygonSmoothEnableMask & DkPolygonFlag_Point) != 0);
	w << CmdInline(3D, LineSmoothEnable{},         (state->polygonSmoothEnableMask & DkPolygonFlag_Line) != 0);
	w << CmdInline(3D, PolygonSmoothEnable{},      (state->polygonSmoothEnableMask & DkPolygonFlag_Fill) != 0);

	w << CmdInline(3D, PolygonOffsetPointEnable{}, (state->depthBiasEnableMask & DkPolygonFlag_Point) != 0);
	w << CmdInline(3D, PolygonOffsetLineEnable{},  (state->depthBiasEnableMask & DkPolygonFlag_Line) != 0);
	w << CmdInline(3D, PolygonOffsetFillEnable{},  (state->depthBiasEnableMask & DkPolygonFlag_Fill) != 0);
}

void dkCmdBufBindColorState(DkCmdBuf obj, DkColorState const* state)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(state);
	CmdBufWriter w{obj};
	w.reserve(5);

	if (state->logicOp == DkLogicOp_Copy)
		w << MacroInline(BindColorBlendEnableState, state->blendEnableMask);
	else
		w << Cmd(3D, ColorLogicOpEnable{}, 1, 0x1500 | state->logicOp);

	if (state->alphaCompareOp == DkCompareOp_Always)
		w << CmdInline(3D, AlphaTestEnable{}, 0);
	else
	{
		w << CmdInline(3D, AlphaTestEnable{}, 1);
		w << CmdInline(3D, AlphaTestFunc{}, state->alphaCompareOp);
	}
}

void dkCmdBufBindColorWriteState(DkCmdBuf obj, DkColorWriteState const* state)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(state);
	CmdBufWriter w{obj};
	w.reserve(2);

	w << Macro(BindColorWriteMasks, state->masks);
}

void dkCmdBufBindBlendStates(DkCmdBuf obj, uint32_t firstId, DkBlendState const states[], uint32_t numStates)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL_ARRAY(states, numStates);
	DK_DEBUG_BAD_INPUT(firstId >= DK_MAX_RENDER_TARGETS || numStates > DK_MAX_RENDER_TARGETS);
	DK_DEBUG_BAD_INPUT(firstId + numStates > DK_MAX_RENDER_TARGETS);
	if (!numStates)
		return;

	CmdBufWriter w{obj};
	w.reserve(7*numStates);

	for (uint32_t i = 0; i < numStates; i ++)
	{
		DkBlendState const& state = states[i];
		w << Cmd(3D, IndependentBlend::EquationRgb{firstId+i},
			state.colorBlendOp,                               // EquationRgb
			getBlendFactorSetting(state.srcColorBlendFactor), // FuncRgbSrc
			getBlendFactorSetting(state.dstColorBlendFactor), // FuncRgbDst
			state.alphaBlendOp,                               // EquationAlpha
			getBlendFactorSetting(state.srcAlphaBlendFactor), // FuncAlphaSrc
			getBlendFactorSetting(state.dstColorBlendFactor)  // FuncAlphaDst
		);
	}
}

void dkCmdBufBindDepthStencilState(DkCmdBuf obj, DkDepthStencilState const* state)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(state);
	CmdBufWriter w{obj};
	w.reserve(3);

	static_assert(sizeof(DkDepthStencilState) == 8, "Bad definition for DkDepthStencilState");

	w << CmdList<1>{ MakeCmdHeader(IncreaseOnce, 2, Subchannel3D, MmeMacroBindDepthStencilState) };
	w.addRawData(state, 8);
}

void dkCmdBufSetDepthBias(DkCmdBuf obj, float constantFactor, float clamp, float slopeFactor)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(6);

	w << Cmd(3D, PolygonOffsetUnits{}, constantFactor*2.0f);
	w << Cmd(3D, PolygonOffsetClamp{}, clamp);
	w << Cmd(3D, PolygonOffsetFactor{}, slopeFactor);
}

void dkCmdBufSetPointSize(DkCmdBuf obj, float size)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(3);

	w << Cmd(3D, PointSpriteSize{}, size);
	w << CmdInline(3D, PipeNop{}, 0); //??
}

void dkCmdBufSetLineWidth(DkCmdBuf obj, float width)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(3);

	// According to nouveau (nvc0_rasterizer_state_create), Maxwell 2nd gen+ only reads LineWidthSmooth.
	// However for consistency with official code, we'll set LineWidthAliased too.
	w << Cmd(3D, LineWidthSmooth{}, width, width);
}

void dkCmdBufSetConservativeRasterEnable(DkCmdBuf obj, bool enable)
{
	DK_ENTRYPOINT(obj);

	CmdBufWriter w{obj};
	w.reserve(5);

	w << CmdInline(3D, PipeNop{}, 0);
	w << CmdInline(3D, PipeNop{}, 0);
	w << CmdInline(3D, PipeNop{}, 0);
	w << CmdInline(3D, SetConservativeRasterEnable{}, enable);
	w << MacroInline(UpdateConservativeRaster, 0);
}

void dkCmdBufSetConservativeRasterDilate(DkCmdBuf obj, float dilate)
{
	DK_ENTRYPOINT(obj);

	int dilateInt = (int)ceilf(dilate*4.0f);
	if (dilateInt < 0) dilateInt = 0;
	else if (dilateInt > 3) dilateInt = 3;

	CmdBufWriter w{obj};
	w.reserve(6);

	w << Macro(WriteHardwareReg, 0x418800, dilateInt<<23, 3<<23);
	w << CmdInline(3D, MmeConservativeRasterDilateEnabled{}, dilateInt>0);
	w << MacroInline(UpdateConservativeRaster, 0);
}

void dkCmdBufSetDepthBounds(DkCmdBuf obj, bool enable, float near, float far)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(4);

	w << CmdInline(3D, DepthBoundsEnable{}, enable);
	if (enable)
		w << Cmd(3D, DepthBoundsNear{}, near, far);
}

void dkCmdBufSetAlphaRef(DkCmdBuf obj, float ref)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(2);

	w << Cmd(3D, AlphaTestRef{}, ref);
}

void dkCmdBufSetBlendConst(DkCmdBuf obj, float red, float green, float blue, float alpha)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(5);

	w << Cmd(3D, BlendConstant{}, red, green, blue, alpha);
}

void dkCmdBufSetStencil(DkCmdBuf obj, DkFace face, uint8_t mask, uint8_t funcRef, uint8_t funcMask)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(6);

	if (unsigned(face) & DkFace_Front)
	{
		w << CmdInline(3D, StencilFrontMask{}, mask);
		w << CmdInline(3D, StencilFrontFuncRef{}, funcRef);
		w << CmdInline(3D, StencilFrontFuncMask{}, funcMask);
	}

	if (unsigned(face) & DkFace_Back)
	{
		w << CmdInline(3D, StencilBackMask{}, mask);
		w << CmdInline(3D, StencilBackFuncRef{}, funcRef);
		w << CmdInline(3D, StencilBackFuncMask{}, funcMask);
	}
}

void dkCmdBufSetTileSize(DkCmdBuf obj, uint32_t width, uint32_t height)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(width < 16 || width > 16384);
	DK_DEBUG_BAD_INPUT(width & (width - 1), "tile width must be a power of two");
	DK_DEBUG_BAD_INPUT(height < 16 || height > 16384);
	DK_DEBUG_BAD_INPUT(height & (height - 1), "tile height must be a power of two");
	CmdBufWriter w{obj};
	w.reserve(2);

	w << Cmd(3D, TiledCacheTileSize{}, E::TiledCacheTileSize::Width{width} | E::TiledCacheTileSize::Height{height});
}

void dkCmdBufTiledCacheOp(DkCmdBuf obj, DkTiledCacheOp op)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(1);

	switch (op)
	{
		default:
		case DkTiledCacheOp_Disable:
			w << CmdInline(3D, TiledCacheEnable{}, 0);
			break;
		case DkTiledCacheOp_Enable:
			w << CmdInline(3D, TiledCacheEnable{}, 1);
			break;
		case DkTiledCacheOp_Flush:
			w << CmdInline(3D, TiledCacheFlush{}, 0);
			break;
		case DkTiledCacheOp_FlushAlt:
			w << CmdInline(3D, TiledCacheFlush{}, 1);
			break;
		case DkTiledCacheOp_UnkDisable:
			w << CmdInline(3D, TiledCacheUnkFeatureEnable{}, 0);
			break;
		case DkTiledCacheOp_UnkEnable:
			w << CmdInline(3D, TiledCacheUnkFeatureEnable{}, 1);
			break;
	}
}

void dkCmdBufSetPatchSize(DkCmdBuf obj, uint32_t size)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(1);

	w << CmdInline(3D, TessellationPatchSize{}, size);
}

void dkCmdBufSetTessOuterLevels(DkCmdBuf obj, float level0, float level1, float level2, float level3)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(5);

	w << Cmd(3D, TessellationOuterLevels{}, level0, level1, level2, level3);
}

void dkCmdBufSetTessInnerLevels(DkCmdBuf obj, float level0, float level1)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(3);

	w << Cmd(3D, TessellationInnerLevels{}, level0, level1);
}
