#pragma once

#define SPDLOG_EOL ""
#define FMT_UNICODE 0

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
#ifndef SIBOX_NO_IMGUI
#include <imgui.h>
#endif

// entt
#include <entt.hpp>

// glm
#include <glm/glm.hpp>

// Core project includes
#include <SDL3/SDL_assert.h>
#include "Core/VulcanalCore.h"
#include "Core/VulcanalLog.h"
