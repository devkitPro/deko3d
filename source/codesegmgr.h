#pragma once
#include "dk_private.h"

namespace dk::detail
{
	class CodeSegMgr : public DkObjBase
	{
		struct Node
		{
			uint32_t m_offset;
			uint32_t m_numPages;
			Node *m_next;
			Node *m_prev;
		};

		static constexpr uint64_t s_segmentSize = 0x100000000UL;
		Mutex m_mutex;
		DkGpuAddr m_segmentIova;
		Node m_root;
		Node m_nodeList;

		Node* allocNode()
		{
			return (Node*)allocMem(sizeof(Node));
		}

		void freeNode(Node* node)
		{
			if (node != &m_root)
				freeMem(node);
		}

		void linkNode(Node* node, Node* after = nullptr)
		{
			Node *ipos = (after ? after : &m_nodeList)->m_next;
			node->m_next = ipos;
			node->m_prev = ipos->m_prev;
			ipos->m_prev = node;
			node->m_prev->m_next = node;
		}

		void unlinkNode(Node* node)
		{
			node->m_prev->m_next = node->m_next;
			node->m_next->m_prev = node->m_prev;
		}

	public:
		constexpr CodeSegMgr(DkDevice device) noexcept : DkObjBase{device},
			m_mutex{}, m_segmentIova{}, m_root{},
			m_nodeList{ .m_next = &m_nodeList, .m_prev = &m_nodeList }
		{ }

		DkResult initialize() noexcept;
		void cleanup() noexcept;

		bool allocSpace(uint32_t size, DkGpuAddr& out_addr) noexcept;
		void freeSpace(DkGpuAddr addr, uint32_t size) noexcept;

		constexpr DkGpuAddr getBase() const noexcept { return m_segmentIova; }
		constexpr uint32_t calcOffset(DkGpuAddr addr) const noexcept
		{
			return uint32_t(addr - m_segmentIova);
		}
	};
}
