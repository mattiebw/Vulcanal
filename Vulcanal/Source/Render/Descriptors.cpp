#include "vulcpch.h"
#include "Render/Descriptors.h"

void DescriptorLayoutBuilder::AddBinding(u32 index, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = index;
    binding.descriptorType = type;
    binding.descriptorCount = 1;

    Bindings.push_back(binding);
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext,
    VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& binding : Bindings)
        binding.stageFlags |= shaderStages;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = pNext;

    layoutInfo.bindingCount = static_cast<u32>(Bindings.size());
    layoutInfo.pBindings = Bindings.data();
    layoutInfo.flags = flags;

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout));
    return layout;
}

void DescriptorAllocator::InitPool(VkDevice device, u32 maxSets, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto& ratio : poolRatios)
    {
        poolSizes.push_back({
           .type = ratio.Type,
            .descriptorCount = static_cast<u32>(ratio.Ratio * static_cast<f32>(maxSets))
        });
    }

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<u32>(poolRatios.size());
    poolInfo.pPoolSizes = poolSizes.data();

    do { VkResult err = vkCreateDescriptorPool(device, &poolInfo, nullptr, &Pool); if (err != VK_SUCCESS) { auto errString = string_VkResult(err); do { static AssertionData data = { false, false, 0, 19, "err == VK_SUCCESS", FullPathToFileName("Descriptors.cpp"), __FUNCTION__, nullptr }; while (!data.AlwaysIgnored && !(err == VK_SUCCESS)) { const AssertState state = ReportAssertion(&data ,"Vulkan error: {}", errString); if (state == AssertState::Retry) { continue; } else if (state == AssertState::Break) { __debugbreak(); } break; } } while ((0,0)); } } while (0);
}

void DescriptorAllocator::ClearDescriptors(VkDevice device) const
{
    vkResetDescriptorPool(device, Pool, 0);
}

void DescriptorAllocator::DestroyPool(VkDevice device)
{
    vkDestroyDescriptorPool(device, Pool, nullptr);
    Pool = nullptr;
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDevice device, VkDescriptorSetLayout layout) const
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = Pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    return descriptorSet;
}
