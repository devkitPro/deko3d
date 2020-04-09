#include "../dk_device.h"
#include "../cmdbuf_writer.h"

#include "engine_3d.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;

void dkCmdBufBindVtxAttribState(DkCmdBuf obj, DkVtxAttribState const attribs[], uint32_t numAttribs)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(numAttribs > DK_MAX_VERTEX_ATTRIBS);
	DK_DEBUG_NON_NULL_ARRAY(attribs, numAttribs);
	CmdBufWriter w{obj};
	w.reserve(1+DK_MAX_VERTEX_ATTRIBS);

	w << CmdList<1> { MakeCmdHeader(Increasing, DK_MAX_VERTEX_ATTRIBS, Subchannel3D, E::VertexAttribState{}) };
	w.addRawData(attribs, numAttribs*sizeof(DkVtxAttribState));

	for (uint32_t i = numAttribs; i < DK_MAX_VERTEX_ATTRIBS; i ++)
	{
		using VS = E::VertexAttribState;
		w << CmdList<1> { VS::IsFixed{} | VS::Size{DkVtxAttribSize_1x32} | VS::Type{DkVtxAttribType_Float} };
	}
}

void dkCmdBufBindVtxBufferState(DkCmdBuf obj, DkVtxBufferState const buffers[], uint32_t numBuffers)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(numBuffers > DK_MAX_VERTEX_BUFFERS);
	DK_DEBUG_NON_NULL_ARRAY(buffers, numBuffers);
	CmdBufWriter w{obj};
	w.reserve(4*numBuffers + 1*(DK_MAX_VERTEX_BUFFERS-numBuffers));

	for (uint32_t i = 0; i < numBuffers; i ++)
	{
		using C = E::VertexArray::Config;
		DkVtxBufferState const& state = buffers[i];
		w << CmdInline(3D, VertexArray::Config{i}, C::Enable{} | C::Stride{state.stride});
		w << CmdInline(3D, IsVertexArrayPerInstance{}+i, state.divisor != 0);
		if (state.divisor)
			w << Cmd(3D, VertexArray::Divisor{i}, state.divisor);
	}

	for (uint32_t i = numBuffers; i < DK_MAX_VERTEX_BUFFERS; i ++)
		w << CmdInline(3D, VertexArray::Config{i}, 0);
}

void dkCmdBufBindVtxBuffers(DkCmdBuf obj, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(firstId > DK_MAX_VERTEX_BUFFERS || numBuffers > DK_MAX_VERTEX_BUFFERS || (firstId+numBuffers) > DK_MAX_VERTEX_BUFFERS);
	DK_DEBUG_NON_NULL_ARRAY(buffers, numBuffers);
	CmdBufWriter w{obj};
	w.reserve(5*numBuffers);

	for (uint32_t i = 0; i < numBuffers; i ++)
	{
		DkBufExtents const& buf = buffers[i];
		DkGpuAddr bufStart = 0x1000;
		DkGpuAddr bufLimit = 0xfff;
		if (buf.size)
		{
			bufStart = buf.addr;
			bufLimit = buf.addr + buf.size - 1;
		}

		w << Cmd(3D, VertexArray::Start{firstId+i}, Iova(bufStart));
		w << Cmd(3D, VertexArrayLimit{}+2*(firstId+i), Iova(bufLimit));
	}
}
