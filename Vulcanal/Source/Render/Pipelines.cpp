#include "vulcpch.h"
#include "Render/Pipelines.h"

bool LoadShaderModule(std::string_view path, VkDevice device, VkShaderModule* outShaderModule)
{
    std::ifstream file(path.data(), std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        VULC_ERROR("Failed to open shader file: {}", path.data());
        return false;
    }

    size_t fileSize = file.tellg();
    u8* data = new u8[fileSize];
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<u32*>(data);

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        return false;
    *outShaderModule = shaderModule;
    return true;
}
