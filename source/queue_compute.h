#pragma once
#include "dk_private.h"
#include "dk_queue.h"

#include "driver_constbuf.h"
#include "maxwell/compute_qmd.h"

namespace dk::detail
{
	class ComputeQueue //: public ObjBase
	{
		Queue& m_parent;
		uint32_t m_curJob;
		uint32_t m_curSmThrottling;

		struct
		{
			maxwell::ComputeQmd qmd;
			ComputeDriverCbuf cbuf;
		} job;

		static constexpr uint32_t s_jobSizeBytes = sizeof(job);
		static constexpr uint32_t s_jobSizeWords = s_jobSizeBytes/sizeof(maxwell::CmdWord);
		static constexpr uint32_t s_jobSizeAlign = (s_jobSizeBytes + 0xFF) &~ 0xFF;

		void initQmd();
		void initComputeEngine();
		void bindConstbuf(uint32_t id, DkGpuAddr addr, uint32_t size);
		void bindShader(CtrlCmdComputeShader const* cmd);
		void bindUniformBuffer(uint32_t id, DkGpuAddr addr, uint32_t size);
		void bindStorageBuffer(uint32_t id, DkGpuAddr addr, uint32_t size);
		void bindTexture(uint32_t id, DkResHandle handle);
		void bindImage(uint32_t id, DkResHandle handle);
		void dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ, DkGpuAddr indirect);

	public:
		ComputeQueue(DkQueue parent) :
			m_parent{*parent}, m_curJob{}, m_curSmThrottling{0x100}, job{}
		{ }

		void initialize();
		CtrlCmdHeader const* processCtrlCmd(CtrlCmdHeader const* cmd);

		void* operator new(size_t size, void* p) noexcept { return p; }
		void operator delete(void* ptr, void* p) noexcept { }
	};
}
