#include <vulcpch.h>

VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags flags)
{
	VkImageSubresourceRange range;
	range.aspectMask     = flags;
	range.baseMipLevel   = 0;
	range.levelCount     = VK_REMAINING_MIP_LEVELS;
	range.baseArrayLayer = 0;
	range.layerCount     = VK_REMAINING_ARRAY_LAYERS;

	return range;
}

void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout,
                     VkImageLayout   newLayout)
{
	VkImageMemoryBarrier2 imageBarrier = {};
	imageBarrier.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.pNext                 = nullptr;

	imageBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;

	VkImageAspectFlags aspectFlags = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		                                 ? VK_IMAGE_ASPECT_DEPTH_BIT
		                                 : VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange = ImageSubresourceRange(aspectFlags);
	imageBarrier.image            = image;

	VkDependencyInfo dependencyInfo = {};
	dependencyInfo.sType            = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext            = nullptr;

	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers    = &imageBarrier;

	vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void BlitImageToImage(VkCommandBuffer commandBuffer, VkImage   source, VkImage destination, VkExtent2D sourceExt,
                      VkExtent2D      destinationExt, VkFilter filter)
{
	VkImageBlit2 blitRegion = {};
	blitRegion.sType        = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blitRegion.pNext        = nullptr;

	blitRegion.srcOffsets[1].x = static_cast<s32>(sourceExt.width);
	blitRegion.srcOffsets[1].y = static_cast<s32>(sourceExt.height);
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = static_cast<s32>(destinationExt.width);
	blitRegion.dstOffsets[1].y = static_cast<s32>(destinationExt.height);
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.mipLevel       = 0;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount     = 1;

	blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.mipLevel       = 0;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount     = 1;

	VkBlitImageInfo2 blitInfo;
	blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blitInfo.pNext = nullptr;

	blitInfo.srcImage       = source;
	blitInfo.dstImage       = destination;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // MW @todo: Lots of hardcoded stuff here!
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.filter         = filter;
	blitInfo.regionCount    = 1;
	blitInfo.pRegions       = &blitRegion;

	vkCmdBlitImage2(commandBuffer, &blitInfo);
}
