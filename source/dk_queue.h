#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "dk_fence.h"
#include "dk_cmdbuf.h"

namespace
{
	template <typename T>
	class RingBuf
	{
		T m_size;
		T m_consumed;
		T m_produced;
	public:
		constexpr RingBuf(T size) noexcept : m_size{size}, m_consumed{}, m_produced{} { }
		constexpr T getFree() const noexcept
		{
			T ret;
			if (m_consumed <= m_produced)
				ret = m_size + m_consumed - m_produced;
			else
				ret = m_consumed - m_produced;
			return ret - 1; // ?? why minus one?
		}
		T reserve(T& pos, T minSize, bool useMax = true) const noexcept
		{
			if ((m_produced + minSize) > m_size)
			{
				if (!reserve(pos, m_size - minSize, false))
					return 0;
				m_produced = 0;
			}
			T numFree = getFree();
			if (numFree < minSize)
				return 0;
			T size = minSize;
			if (useMax)
			{
				if (m_consumed <= m_produced)
					size = m_size - m_produced - !m_consumed; // ?? why this term
				else
					size = numFree;
			}
			pos = m_produced;
			return size;
		}
		void produce(T size) noexcept
		{
			T pos = m_produced + size;
			if (pos >= m_size)
				pos -= m_size;
			m_produced = pos;
		}
		void consume(T size) noexcept
		{
			T pos = m_consumed + size;
			if (pos >= m_size)
				pos -= m_size;
			m_consumed = pos;
		}
	};
}

class tag_DkQueue : public DkObjBase
{
	tag_DkMemBlock m_workBufMemBlock;
	tag_DkCmdBuf m_cmdBuf;

	void onCmdBufAddMem(size_t minReqSize) noexcept;
	static void _addMemFunc(void* userData, DkCmdBuf cmdbuf, size_t minReqSize) noexcept
	{
		static_cast<tag_DkQueue*>(userData)->onCmdBufAddMem(minReqSize);
	}

public:
	constexpr tag_DkQueue(DkDevice dev) : DkObjBase{dev},
		m_workBufMemBlock{dev}, m_cmdBuf{{dev,this,_addMemFunc}} { }

};
