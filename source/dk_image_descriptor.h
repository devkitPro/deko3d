#include "dk_image.h"

#include "maxwell/texture_image_control_block.h"

struct DkImageDescriptor : public maxwell::TextureImageControl
{
};

DK_OPAQUE_CHECK(DkImageDescriptor);
