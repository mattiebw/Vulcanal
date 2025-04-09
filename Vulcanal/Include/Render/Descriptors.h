#pragma once

struct DescriptorLayoutBuilder
{
    void AddBinding(u32 index, VkDescriptorType type);
    void Clear() { Bindings.clear(); }
    VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr,
        VkDescriptorSetLayoutCreateFlags flags = 0);

protected:
    std::vector<VkDescriptorSetLayoutBinding> Bindings;
};

struct PoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
};

struct DescriptorAllocator
{
    void InitPool(VkDevice device, u32 maxSets, std::span<PoolSizeRatio> poolRatios);
    void ClearDescriptors(VkDevice device) const;
    void DestroyPool(VkDevice device);
    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout) const;

    NODISCARD FORCEINLINE VkDescriptorPool GetPool() const { return Pool; }
    
protected:
    VkDescriptorPool Pool = nullptr;
};
