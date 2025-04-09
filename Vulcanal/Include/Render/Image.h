#pragma once

struct AllocatedImage
{
    VkImage Image;
    VkImageView ImageView;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;

    void Reset()
    {
        Image = VK_NULL_HANDLE;
        ImageView = VK_NULL_HANDLE;
        Allocation = VK_NULL_HANDLE;
        Extent = { 0, 0, 0 };
        Format = VK_FORMAT_B8G8R8A8_SRGB;
    }
};
