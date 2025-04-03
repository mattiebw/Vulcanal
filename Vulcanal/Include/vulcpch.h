#pragma once

#define SPDLOG_EOL ""
#define FMT_UNICODE 0

#define VULC_NO_IMGUI // temporarily disable ImGui, while we get the Vulkan renderer working

// Utility functions and types
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <filesystem>

// Data types
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// imgui
#ifndef VULC_NO_IMGUI
#include <imgui.h>
#endif

// entt
#include <entt.hpp>

// glm
#include <glm/glm.hpp>

// Vulkan
#include "vulkan/vulkan.h"
#include "vulkan/vk_enum_string_helper.h"

// Vk-Bootstrap
#include <vkbootstrap/VkBootstrap.h>
#include <vkbootstrap/VkBootstrapDispatch.h>

// VMA
#include <vk_mem_alloc.h>

// fmt
#include <spdlog/fmt/fmt.h>

// Core project includes
#include "Core/VulcanalLog.h" // Include the logger first, so we can use it in the assertion macro.
#include <SDL3/SDL_assert.h> // Even though we use our own assertion system, we need this for SDL_TriggerBreakpoint!
#include "Core/Assert.h"
#include "Core/VulcanalCore.h"
#include "Core/Delegate.h"
#include "Render/VulkanUtil.h"
