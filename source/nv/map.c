#include "map.h"

static u32 g_nvmap_fd = -1;
static u64 g_refCnt;

Result nvMapInit(void)
{
	Result rc;

	if (atomicIncrement64(&g_refCnt) > 0)
		return 0;

	rc = nvOpen(&g_nvmap_fd, "/dev/nvmap");

	if (R_FAILED(rc))
		atomicDecrement64(&g_refCnt);

	return rc;
}

void nvMapExit(void)
{
	if (atomicDecrement64(&g_refCnt) == 0)
	{
		if (g_nvmap_fd != -1)
			nvClose(g_nvmap_fd);

		g_nvmap_fd = -1;
	}
}

u32 nvMapGetFd(void)
{
	return g_nvmap_fd;
}

Result nvMapAlloc(NvMap* m, void* cpu_addr, u32 size, u32 align, NvKind kind, bool is_cpu_cacheable)
{
	Result rc;

	if (cpu_addr == NULL)
		return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

	size = (size + 0xFFF) &~ 0xFFF;
	m->has_init = true;
	m->is_cpu_cacheable = is_cpu_cacheable;
	m->size = size;
	m->handle = -1;
	m->cpu_addr = cpu_addr;
	m->kind = kind;

	rc = nvioctlNvmap_Create(g_nvmap_fd, size, &m->handle);

	if (R_SUCCEEDED(rc))
		rc = nvioctlNvmap_Alloc(g_nvmap_fd, m->handle,
			0, is_cpu_cacheable ? 1 : 0, align, kind, m->cpu_addr);

	if (R_SUCCEEDED(rc) && !is_cpu_cacheable) {
		armDCacheFlush(m->cpu_addr, m->size);
		svcSetMemoryAttribute(m->cpu_addr, m->size, 8, 8);
    }

	if (R_FAILED(rc))
		nvMapFree(m);

	return rc;
}

void nvMapFree(NvMap* m)
{
	if (!m->has_init)
		return;

	if (m->handle != -1) {
		nvioctlNvmap_Free(g_nvmap_fd, m->handle);
		m->handle = -1;
	}

	if (m->cpu_addr) {
		if (!m->is_cpu_cacheable)
			svcSetMemoryAttribute(m->cpu_addr, m->size, 8, 0);
		m->cpu_addr = NULL;
	}

	m->has_init = false;
}
