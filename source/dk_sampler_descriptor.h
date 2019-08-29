#include "dk_private.h"

#include "maxwell/texture_sampler_control_block.h"

struct DkSamplerDescriptor : public maxwell::TextureSamplerControl
{
};

DK_OPAQUE_CHECK(DkSamplerDescriptor);
