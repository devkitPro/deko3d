#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <deko3d.h>

#ifdef DEBUG
#define DK_ERROR_CONTEXT(_ctx) (_ctx)
#else
#define DK_ERROR_CONTEXT(_ctx) nullptr
#endif

#define DK_FUNC_ERROR_CONTEXT DK_ERROR_CONTEXT(__func__)
