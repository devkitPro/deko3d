#pragma once
#include <switch.h>

typedef struct NvMap
{
	u32    handle;
	u32    size;
	void*  cpu_addr;
	NvKind kind;
    bool   has_init;
	bool   is_cpu_cacheable;
} NvMap;

#ifdef __cplusplus
extern "C" {
#endif

Result nvMapInit(void);
u32 nvMapGetFd(void);
void nvMapExit(void);

Result nvMapAlloc(NvMap* m, void* cpu_addr, u32 size, u32 align, NvKind kind, bool is_cpu_cacheable);
void nvMapFree(NvMap* m);

static inline u32 nvMapGetHandle(NvMap* m)
{
	return m->handle;
}

static inline void* nvMapGetCpuAddr(NvMap* m)
{
	return m->cpu_addr;
}

#ifdef __cplusplus
}
#endif
