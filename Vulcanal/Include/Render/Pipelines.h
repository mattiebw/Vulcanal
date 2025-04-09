#pragma once

bool LoadShaderModule(std::string_view path, VkDevice device, VkShaderModule* outShaderModule);
