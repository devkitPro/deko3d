#include "codesegmgr.h"
#include "dk_device.h"

using namespace dk::detail;

DkResult CodeSegMgr::initialize()
{
	Result rc = nvAddressSpaceAlloc(getDevice()->getAddrSpace(), false, s_segmentSize, &m_segmentIova);
	if (R_FAILED(rc))
		return DkResult_Fail;

	m_root.m_offset   = 0;
	m_root.m_numPages = s_segmentSize / getDevice()->getGpuInfo().bigPageSize;
	linkNode(&m_root);

	return DkResult_Success;
}

void CodeSegMgr::cleanup()
{
	if (m_nodeList.m_next == &m_nodeList)
		return;

	Node *next;
	for (Node *pos = m_nodeList.m_next; pos != &m_nodeList; pos = next)
	{
		next = pos->m_next;
		freeNode(pos);
	}

	nvAddressSpaceFree(getDevice()->getAddrSpace(), m_segmentIova, s_segmentSize);
}

bool CodeSegMgr::allocSpace(uint32_t size, DkGpuAddr& out_addr)
{
	uint32_t bigPageSize = getDevice()->getGpuInfo().bigPageSize;
	uint32_t numPages = (size + bigPageSize - 1) / bigPageSize;
	MutexHolder m{m_mutex};

	// Find a node in the list with enough space.
	Node *node;
	for (node = m_nodeList.m_next; node != &m_nodeList; node = node->m_next)
		if (node->m_numPages >= numPages)
			break;

	// Bail out if we reached the end of the list before finding a suitable node.
	if (node == &m_nodeList)
		return false;

	out_addr = m_segmentIova + bigPageSize*node->m_offset;
	if (node->m_numPages == numPages)
	{
		// Sizes match exactly, so remove and free this node
		unlinkNode(node);
		freeNode(node);
	}
	else
	{
		// Trim down the size of this node
		node->m_offset   += numPages;
		node->m_numPages -= numPages;
	}

	return true;
}

void CodeSegMgr::freeSpace(DkGpuAddr addr, uint32_t size) noexcept
{
	uint32_t bigPageSize = getDevice()->getGpuInfo().bigPageSize;
	uint32_t numPages = (size + bigPageSize - 1) / bigPageSize;
	MutexHolder m{m_mutex};
	// TODO
	(void)bigPageSize;
	(void)numPages;
}
