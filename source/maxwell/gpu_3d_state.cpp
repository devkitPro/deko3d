#include "../dk_device.h"
#include "../cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;
using SRC = E::MmeShadowRamControl;

void dkCmdBufBindRasterizerState(DkCmdBuf obj, DkRasterizerState const* state)
{
#ifdef DEBUG
	if (!state)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	CmdBufWriter w{obj};
	w.reserve(17);

	w << CmdInline(3D, ViewVolumeClipControl{}, state->depthClampEnable ? 0x101A : 0x181D); // todo: figure out what this actually does
	w << CmdInline(3D, RasterizerEnable{}, !state->rasterizerDiscardEnable);

	static const uint16_t polygonModes[] = { E::SetPolygonModeFront::Point, E::SetPolygonModeFront::Line, E::SetPolygonModeFront::Fill, 0 };
	w << CmdInline(3D, SetPolygonModeFront{}, polygonModes[state->polygonMode]);
	w << CmdInline(3D, SetPolygonModeBack{},  polygonModes[state->polygonMode]);

	w << CmdInline(3D, CullFaceEnable{}, state->cullMode != DkFace_None);
	if (state->cullMode != DkFace_None)
	{
		static const uint16_t cullModes[] = { E::SetCullFace::Front, E::SetCullFace::Back, E::SetCullFace::FrontAndBack };
		w << CmdInline(3D, SetCullFace{}, cullModes[state->cullMode-1]);
	}

	static const uint16_t frontFaces[] = { E::SetFrontFace::CW, E::SetFrontFace::CCW };
	w << CmdInline(3D, SetFrontFace{}, frontFaces[state->frontFace]);

	w << CmdInline(3D, PolygonOffsetPointEnable{} + state->polygonMode, state->depthBiasEnable);
	if (state->depthBiasEnable)
	{
		w << Cmd(3D, PolygonOffsetUnits{}, state->depthBiasConstantFactor*2.0f);
		w << Cmd(3D, PolygonOffsetClamp{}, state->depthBiasClamp);
		w << Cmd(3D, PolygonOffsetFactor{}, state->depthBiasSlopeFactor);
	}

	w << Cmd(3D, LineWidthSmooth{}, state->lineWidth, state->lineWidth);
}
