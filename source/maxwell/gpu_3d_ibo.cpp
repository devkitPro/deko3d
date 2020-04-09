#include "../dk_device.h"
#include "../cmdbuf_writer.h"

#include "engine_3d.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;

void dkCmdBufBindIdxBuffer(DkCmdBuf obj, DkIdxFormat format, DkGpuAddr address)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(4);

	w << Cmd(3D, IndexArrayStartIova{}, Iova(address));
	w << CmdInline(3D, IndexArrayFormat{}, format);
}

void dkCmdBufSetPrimitiveRestart(DkCmdBuf obj, bool enable, uint32_t index)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(3);

	w << CmdInline(3D, PrimitiveRestartEnable{}, enable);
	if (enable)
		w << Cmd(3D, PrimitiveRestartIndex{}, index);
}
