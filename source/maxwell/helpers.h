#pragma once
#include "command.h"

#define BindEngine(_engine) \
	::maxwell::MakeNonIncreasingCmd(::maxwell::Subchannel##_engine, 0, ::maxwell::ClassId<::maxwell::Engine##_engine>)

#define __Cmd(_mode, _engine, _reg, ...) \
	::maxwell::Make##_mode##Cmd(::maxwell::Subchannel##_engine, ::maxwell::Engine##_engine::_reg, __VA_ARGS__)

#define __Macro(_mode, _name, ...) \
	::maxwell::Make##_mode##Cmd(::maxwell::Subchannel3D, ::MmeMacro##_name, __VA_ARGS__)

#define Cmd(_engine, _reg, ...) __Cmd(Increasing, _engine, _reg, __VA_ARGS__)
#define CmdInline(_engine, _reg, ...) __Cmd(Inline, _engine, _reg, __VA_ARGS__)
#define CmdPipe(_engine, _reg, ...) __Cmd(NonIncreasing, _engine, _reg, __VA_ARGS__)
#define CmdUpload(_engine, _reg, ...) __Cmd(IncreaseOnce, _engine, _reg, __VA_ARGS__)

#define Macro(_name, ...) __Macro(IncreaseOnce, _name, __VA_ARGS__)
#define MacroInline(_name, ...) __Macro(Inline, _name, __VA_ARGS__)

namespace maxwell
{
	constexpr unsigned Subchannel3D = 0;
	constexpr unsigned SubchannelCompute = 1;
	constexpr unsigned SubchannelInline = 2;
	constexpr unsigned Subchannel2D = 3;
	constexpr unsigned SubchannelDMA = 4;
	constexpr unsigned SubchannelGpfifo = 6;
}
