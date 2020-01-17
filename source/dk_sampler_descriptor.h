#include "dk_private.h"

#include "maxwell/texture_sampler_control_block.h"

namespace dk::detail
{

struct SamplerDescriptor : public maxwell::TextureSamplerControl
{
};

}

DK_OPAQUE_CHECK(SamplerDescriptor);
