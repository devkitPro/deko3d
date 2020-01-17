#include "dk_image.h"

#include "maxwell/texture_image_control_block.h"

namespace dk::detail
{

struct ImageDescriptor : public maxwell::TextureImageControl
{
};

}

DK_OPAQUE_CHECK(ImageDescriptor);
