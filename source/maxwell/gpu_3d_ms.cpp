#include "../dk_device.h"
#include "../cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"

#include "../driver_constbuf.h"
#include "texture_image_control_block.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;
using SRC = E::MmeShadowRamControl;

namespace
{
	constexpr uint32_t locationsMS1[] = { 0x88888888, 0x88888888, 0x88888888, 0x88888888 };
	constexpr uint32_t locationsMS2[] = { 0x44CC44CC, 0x44CC44CC, 0x44CC44CC, 0x44CC44CC };
	constexpr uint32_t locationsMS4[] = { 0xEAA26E26, 0xEAA26E26, 0xEAA26E26, 0xEAA26E26 };
	constexpr uint32_t locationsMS8[] = { 0x359DB759, 0x1FFB71D3, 0x359DB759, 0x1FFB71D3 };
	//constexpr uint32_t locationsMS16[] = { 0x7CA55799, 0x3BBDDA63, 0xC22418E6, 0x01FE4F80 };

	constexpr uint32_t paramsMS1[] = { 0x0, 0, 0, 0x0 };
	constexpr uint32_t paramsMS2[] = { 0x1, 1, 1, 0x3 };
	constexpr uint32_t paramsMS4[] = { 0x3, 2, 1, 0x1 };
	constexpr uint32_t paramsMS8[] = { 0x7, 3, 0, 0x1 };
	//constexpr uint32_t paramsMS16[] = { 0xF, 0, 0, 0x0 };

	constexpr unsigned encodeSampleLocationValue(float x)
	{
		if (x < 0.0f) x = 0.0f;
		else if (x > 0.9375f) x = 0.9375f;
		return unsigned(x*16.0f + 0.5f);
	}

	constexpr unsigned encodeSampleLocation(DkSampleLocation const& loc)
	{
		unsigned x = encodeSampleLocationValue(loc.x);
		unsigned y = encodeSampleLocationValue(loc.y);
		return x | (y<<4);
	}

	constexpr unsigned encodeModulationTableValue(float value)
	{
		if (value < 0.0f) value = 0.0f;
		else if (value > 1.0f) value = 1.0f;
		unsigned ret = unsigned(value*16.0f + 0.5f);
		return ret<<3;
	}
}

void dkMultisampleStateSetLocations(DkMultisampleState* obj, DkSampleLocation const* locations, uint32_t numLocations)
{
	DkMsMode mode = obj->mode;
	if (obj->rasterizerMode > mode)
		mode = obj->rasterizerMode;

	if (!locations) switch (mode)
	{
		default:
		case DkMsMode_1x:
			memcpy(obj->sampleLocations, locationsMS1, sizeof(obj->sampleLocations));
			break;
		case DkMsMode_2x:
			memcpy(obj->sampleLocations, locationsMS2, sizeof(obj->sampleLocations));
			break;
		case DkMsMode_4x:
			memcpy(obj->sampleLocations, locationsMS4, sizeof(obj->sampleLocations));
			break;
		case DkMsMode_8x:
			memcpy(obj->sampleLocations, locationsMS8, sizeof(obj->sampleLocations));
			break;
		/* Currently not supported, maybe in the future
		case DkMsMode_16x:
			memcpy(obj->sampleLocations, locationsMS16, sizeof(obj->sampleLocations));
			break;
		*/
	}
	else if (numLocations)
	{
		u8 encoded[16];
		unsigned j = 0;
		for (unsigned i = 0; i < 16; i ++)
		{
			encoded[i] = encodeSampleLocation(locations[j++]);
			if (j == numLocations)
				j = 0;
		}
		memcpy(obj->sampleLocations, encoded, sizeof(obj->sampleLocations));
	}
}

void dkCmdBufBindMultisampleState(DkCmdBuf obj, DkMultisampleState const* state)
{
#ifdef DEBUG
	if (!state)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	CmdBufWriter w{obj};
	w.reserve(12);

	w << CmdInline(3D, MultisampleControl{}, E::MultisampleControl::AlphaToCoverage{state->alphaToCoverageEnable});
	w << CmdInline(3D, MultisampleEnable{}, state->mode || state->rasterizerMode);
	w << CmdInline(3D, AlphaToCoverageDither{}, state->alphaToCoverageDither);

	w << Cmd(3D, MultisampleSampleLocations{},
		state->sampleLocations[0], state->sampleLocations[1], state->sampleLocations[2], state->sampleLocations[3]);

	// TODO: update driver constbuf with sample locations lut

	if (state->rasterizerMode > state->mode)
	{
		w << CmdInline(3D, SetMultisampleRasterEnable{}, 1);
		w << CmdInline(3D, MultisampleRasterSamples{}, getMsaaMode(state->rasterizerMode));
		w << CmdInline(3D, CoverageModulationMode{}, state->coverageModulation);
	}
	else
		w << CmdInline(3D, SetMultisampleRasterEnable{}, 0);

	using C = E::MultisampleCoverageToColor;
	w << CmdInline(3D, MultisampleCoverageToColor{},
		C::Enable{state->coverageToColorEnable} | C::Target{state->coverageToColorOutput});
}

void dkCmdBufSetSampleMask(DkCmdBuf obj, uint32_t mask)
{
	CmdBufWriter w{obj};
	w.reserve(5);

	w << Cmd(3D, MultisampleSampleMask{}, mask, mask, mask, mask);
}

void dkCmdBufSetCoverageModulationTable(DkCmdBuf obj, float const table[16])
{
	CmdBufWriter w{obj};
	w.reserve(6);

	if (!table)
		w << CmdInline(3D, SetCoverageModulationTableEnable{}, 0);
	else
	{
		uint32_t encoded[4];
		for (unsigned i = 0; i < 16; i += 4)
		{
			unsigned a0 = encodeModulationTableValue(table[i+0]);
			unsigned a1 = encodeModulationTableValue(table[i+1]);
			unsigned a2 = encodeModulationTableValue(table[i+2]);
			unsigned a3 = encodeModulationTableValue(table[i+3]);
			encoded[i/4] = a0 | (a1<<8) | (a2<<16) | (a3<<24);
		}
		w << CmdInline(3D, SetCoverageModulationTableEnable{}, 1);
		w << Cmd(3D, CoverageModulationTable{},
			encoded[0], encoded[1], encoded[2], encoded[3]);
	}
}

void dkCmdBufResolveDepthValues(DkCmdBuf obj)
{
	CmdBufWriter w{obj};
	w.reserve(5);

	w << CmdInline(3D, ResolveDepthValues{}, 1);
}
