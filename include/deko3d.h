// Temporary header file which will be replaced once iface generation works

#pragma once
#include <stddef.h>
#include <stdint.h>

#if __cplusplus >= 201402L
#define DK_CONSTEXPR constexpr
#else
#define DK_CONSTEXPR static inline
#endif

#define DK_DECL_OPAQUE(_typename) \
	typedef struct tag_##_typename *_typename

DK_DECL_OPAQUE(DkDevice);
DK_DECL_OPAQUE(DkMemBlock);
DK_DECL_OPAQUE(DkFence);
DK_DECL_OPAQUE(DkQueue);
DK_DECL_OPAQUE(DkShader);
DK_DECL_OPAQUE(DkImage);
DK_DECL_OPAQUE(DkImageView);
DK_DECL_OPAQUE(DkImagePool);
DK_DECL_OPAQUE(DkSampler);
DK_DECL_OPAQUE(DkCmdBuf);
DK_DECL_OPAQUE(DkWindow);

typedef enum
{
	DkResult_Success = 0,
	DkResult_Fail = 1,
	DkResult_NotImplemented = 2,
	DkResult_OutOfMemory = 3,
} DkResult;

typedef void (*DkErrorFunc)(void* userData, DkResult result);
typedef DkResult (*DkAllocFunc)(void* userData, size_t alignment, size_t size, void** out);
typedef void (*DkFreeFunc)(void* userData, void* mem);

enum
{
	DkDeviceFlags_
};

typedef struct DkDeviceMaker
{
	void* userData;
	DkErrorFunc cbError;
	DkAllocFunc cbAlloc;
	DkFreeFunc cbFree;
	uint32_t flags;
} DkDeviceMaker;

DK_CONSTEXPR void dkDeviceMakerDefaults(DkDeviceMaker* maker)
{
	maker->userData = NULL;
	maker->cbError = NULL;
	maker->cbAlloc = NULL;
	maker->cbFree = NULL;
	maker->flags = 0;
}

#ifdef __cplusplus
extern "C" {
#endif

DkDevice dkDeviceCreate(DkDeviceMaker const* maker);
void dkDeviceDestroy(DkDevice obj);

#ifdef __cplusplus
}
#endif
