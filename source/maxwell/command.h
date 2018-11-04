#pragma once
#include <stdint.h>
#include <utility>

namespace maxwell
{

template <uint32_t... values>
using u32_seq = std::integer_sequence<uint32_t, values...>;

template <uint32_t value>
using make_u32_seq = std::make_integer_sequence<uint32_t, value>;

union CmdWord
{
	uint32_t i;
	float f;

	constexpr CmdWord() : i{} { }
	explicit constexpr CmdWord(float f) : f{f} { }
	template <typename T>
	explicit constexpr CmdWord(T&& t) : i{uint32_t(t)} { }
};

template <uint32_t size>
struct CmdList
{
	CmdWord raw[size];

	constexpr uint32_t getSize() const noexcept { return size; }

	CmdList() = delete;
	CmdList(CmdList const&) = delete;
	CmdList(CmdList&&) = delete;
	CmdList& operator=(CmdList const&) = default;
	CmdList& operator=(CmdList&&) = default;

	template <typename... Targs>
	constexpr CmdList(Targs&&... args) noexcept : raw{CmdWord{args}...} { }

	template <uint32_t sizeA>
	explicit constexpr CmdList(CmdList<sizeA>&& a, CmdList<size-sizeA>&& b) noexcept :
		CmdList{ std::move(a), std::move(b), make_u32_seq<sizeA>{}, make_u32_seq<size-sizeA>{} } { }

	constexpr CmdList& copyTo(CmdWord* p) const noexcept
	{
		return *reinterpret_cast<CmdList*>(p) = *this;
	}

	constexpr CmdList& moveTo(CmdWord* p) noexcept
	{
		return *reinterpret_cast<CmdList*>(p) = std::move(*this);
	}

private:
	template <uint32_t sizeA, uint32_t... idxA, uint32_t... idxB>
	explicit constexpr CmdList(CmdList<sizeA>&& a, CmdList<size-sizeA>&& b, u32_seq<idxA...>, u32_seq<idxB...>) noexcept :
		raw{ std::move(a.raw[idxA])..., std::move(b.raw[idxB])... } { }
};

template <uint32_t sizeA, uint32_t sizeB>
constexpr auto operator +(CmdList<sizeA>&& a, CmdList<sizeB>&& b)
{
	return CmdList<sizeA+sizeB>{ std::move(a), std::move(b) };
}

enum SubmissionMode : uint32_t
{
	Increasing       = 1,
	NonIncreasing    = 3,
	Inline           = 4,
	IncreaseOnce     = 5,
};

constexpr uint32_t ShiftField(uint32_t field, uint32_t pos, uint32_t size)
{
	field &= (uint32_t{1} << size) - 1;
	return field << pos;
}

constexpr uint32_t MakeCmdHeader(SubmissionMode mode, uint16_t arg, uint8_t subchannel, uint16_t method)
{
	return ShiftField(method, 0, 13) | ShiftField(subchannel, 13, 3) | ShiftField(arg, 16, 13) | ShiftField(mode, 29, 3);
}

template <typename... Targs>
constexpr auto MakeCmd(uint32_t header, Targs&&... args)
{
	return CmdList<1+sizeof...(args)>{ header, std::forward<Targs>(args)... };
}

constexpr auto MakeInlineCmd(uint8_t subchannel, uint16_t method, uint16_t arg)
{
	return MakeCmd(MakeCmdHeader(Inline, arg, subchannel, method));
}

template <typename... Targs>
constexpr auto MakeIncreasingCmd(uint8_t subchannel, uint16_t method, Targs&&... args)
{
	return MakeCmd(MakeCmdHeader(Increasing, sizeof...(args), subchannel, method), std::forward<Targs>(args)...);
}

template <typename... Targs>
constexpr auto MakeNonIncreasingCmd(uint8_t subchannel, uint16_t method, Targs&&... args)
{
	return MakeCmd(MakeCmdHeader(NonIncreasing, sizeof...(args), subchannel, method), std::forward<Targs>(args)...);
}

template <typename... Targs>
constexpr auto MakeIncreaseOnceCmd(uint8_t subchannel, uint16_t method, Targs&&... args)
{
	return MakeCmd(MakeCmdHeader(IncreaseOnce, sizeof...(args), subchannel, method), std::forward<Targs>(args)...);
}

}
