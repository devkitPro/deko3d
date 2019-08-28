#include "../dk_device.h"
#include "image_formats.h"

using namespace dk::detail;
using namespace maxwell;

void tag_DkDevice::calcZcullStorageInfo(dk::detail::ZcullStorageInfo& out, uint32_t width, uint32_t height, uint32_t depth, DkImageFormat format, DkMsMode msMode)
{
	FormatTraits const& traits = formatTraits[format];
	const nvioctl_zcull_info& zcullInfo = *getGpuInfo().zcullInfo;

	out.zetaType = traits.stencilBits ? 1 : 2;

	out.width  = (width  + zcullInfo.width_align_pixels - 1)  / zcullInfo.width_align_pixels  * zcullInfo.width_align_pixels;
	out.height = (height + zcullInfo.height_align_pixels - 1) / zcullInfo.height_align_pixels * zcullInfo.height_align_pixels;
	out.depth  = depth;

	out.layerSize = (out.width * out.height + zcullInfo.pixel_squares_by_aliquots - 1) / zcullInfo.pixel_squares_by_aliquots;
	out.imageSize = out.layerSize * out.depth;
	out.totalSize = zcullInfo.region_header_size + out.imageSize * zcullInfo.region_byte_multiplier + zcullInfo.subregion_header_size;
}
