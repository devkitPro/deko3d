#pragma once
#include "dk_cmdbuf.h"

#include "maxwell/helpers.h"

namespace dk::detail
{
	template <bool StreamOpReserves = false>
	class CmdBufWriter
	{
		DkCmdBuf m_cmdBuf;
		maxwell::CmdWord* m_pos;
		bool m_dirty;

		maxwell::CmdWord* getPos() noexcept
		{
			if (!m_dirty)
			{
				m_pos = m_cmdBuf->m_cmdPos;
				m_dirty = true;
			}
			return m_pos;
		}

	public:
		CmdBufWriter(DkCmdBuf buf) noexcept :
			m_cmdBuf{buf}, m_pos{}, m_dirty{} { }
		~CmdBufWriter() { flush(); }

		void invalidate() noexcept
		{
			m_dirty = false;
		}

		void flush(bool doInvalidate = false) noexcept
		{
			if (m_dirty)
			{
				m_cmdBuf->m_cmdPos = m_pos;
				if (doInvalidate)
					invalidate();
			}
		}

		maxwell::CmdWord* reserve(uint32_t size) noexcept
		{
			flush();
			maxwell::CmdWord *pos = getPos();
			if ((pos + size) >= m_cmdBuf->m_cmdEnd)
				m_pos = pos = m_cmdBuf->requestCmdMem(size);
			return pos;
		}

		void split(uint32_t flags = dk::detail::CtrlCmdGpfifoEntry::AutoKick)
		{
			flush();
			m_cmdBuf->signOffGpfifoEntry(flags);
		}

		template <typename T>
		T* addCtrl()
		{
			split();
			return static_cast<T*>(m_cmdBuf->appendCtrlCmd(sizeof(T)));
		}

		void addRaw(DkGpuAddr iova, uint32_t numCmds, uint32_t flags)
		{
			split();
			m_cmdBuf->appendRawGpfifoEntry(iova, numCmds, flags);
		}

		template <uint32_t size>
		void add(maxwell::CmdList<size> const& cmds)
		{
			cmds.copyTo(getPos());
			m_pos += size;
		}

		template <uint32_t size>
		void add(maxwell::CmdList<size>&& cmds)
		{
			cmds.moveTo(getPos());
			m_pos += size;
		}

		template <uint32_t... sizes>
		void add(maxwell::CmdList<sizes>&&... cmds)
		{
			auto list = Cmds(std::move(cmds)...);
			add(std::move(list));
		}

		template <uint32_t size>
		void reserveAdd(maxwell::CmdList<size> const& cmds)
		{
			reserve(size);
			add(cmds);
		}

		template <uint32_t size>
		void reserveAdd(maxwell::CmdList<size>&& cmds)
		{
			reserve(size);
			add(std::move(cmds));
		}

		template <uint32_t... sizes>
		void reserveAdd(maxwell::CmdList<sizes>&&... cmds)
		{
			auto list = Cmds(std::move(cmds)...);
			reserveAdd(std::move(list));
		}

		template <uint32_t size>
		CmdBufWriter& operator<<(maxwell::CmdList<size> const& cmds)
		{
			if constexpr(!StreamOpReserves)
				add(cmds);
			else
				reserveAdd(cmds);
			return *this;
		}

		template <uint32_t size>
		CmdBufWriter& operator<<(maxwell::CmdList<size>&& cmds)
		{
			if constexpr(!StreamOpReserves)
				add(std::move(cmds));
			else
				reserveAdd(std::move(cmds));
			return *this;
		}
	};

	using CmdBufWriterChecked = CmdBufWriter<true>;
}
