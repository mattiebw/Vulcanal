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

// Vk-Bootstrap
#include <vkbootstrap/VkBootstrap.h>
#include <vkbootstrap/VkBootstrapDispatch.h>

// VMA
#include <vk_mem_alloc.h>

// Core project includes
#include <SDL3/SDL_assert.h>
#include "Core/VulcanalCore.h"
#include "Core/VulcanalLog.h"
#include "Core/Delegate.h"
