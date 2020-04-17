#include "../dk_device.h"
#include "image_formats.h"

using namespace dk::detail;
using namespace maxwell;

void Device::calcZcullStorageInfo(ZcullStorageInfo& out, uint32_t width, uint32_t height, uint32_t depth, DkImageFormat format, DkMsMode msMode)
{
	const nvioctl_zcull_info& zcullInfo = *getGpuInfo().zcullInfo;

	// Note: Old official software would select a different Zcull format for depth targets
	// having stencil components. In more recent official software, this special Zcull stencil
	// mode must be manually enabled with a flag coming from the image. Presumably this was
	// done as most users won't actually want to spend Zcull resources on storing stencil data.
	// TODO: add and implement a flag to do just that
	//FormatTraits const& traits = formatTraits[format];
	//out.zetaType = traits.stencilBits ? 1 : 2;
	out.zetaType = 2;

	out.width  = (width  + zcullInfo.width_align_pixels - 1)  / zcullInfo.width_align_pixels  * zcullInfo.width_align_pixels;
	out.height = (height + zcullInfo.height_align_pixels - 1) / zcullInfo.height_align_pixels * zcullInfo.height_align_pixels;
	out.depth  = depth;

	out.layerSize = (out.width * out.height + zcullInfo.pixel_squares_by_aliquots - 1) / zcullInfo.pixel_squares_by_aliquots;
	out.imageSize = out.layerSize * out.depth;
	out.totalSize = zcullInfo.region_header_size + out.imageSize * zcullInfo.region_byte_multiplier + zcullInfo.subregion_header_size;
}
