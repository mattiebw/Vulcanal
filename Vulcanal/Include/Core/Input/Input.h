#pragma once

#include <SDL3/SDL_events.h>

#include "Scancode.h"
#include "MouseButtons.h"

class Input
{
public:
	static void Init();
	static void Shutdown();

	NODISCARD static FORCEINLINE bool IsKeyDown(const Scancode key)
	{
		return !s_ImGuiHasKeyboardFocus && s_Keys[static_cast<u16>(key)];
	}

	NODISCARD static FORCEINLINE bool IsKeyDownThisFrame(const Scancode key)
	{
		return !s_ImGuiHasKeyboardFocus && s_Keys[static_cast<u16>(key)] && !s_PreviousKeys[static_cast<u16>(key)];
	}

	NODISCARD static FORCEINLINE bool IsKeyUp(const Scancode key)
	{
		return !s_ImGuiHasKeyboardFocus && !s_Keys[static_cast<u16>(key)];
	}

	NODISCARD static FORCEINLINE bool IsKeyUpThisFrame(const Scancode key)
	{
		return !s_ImGuiHasKeyboardFocus && !s_Keys[static_cast<u16>(key)] && s_PreviousKeys[static_cast<u16>(key)];
	}

	NODISCARD static FORCEINLINE bool IsMouseButtonDown(const MouseButton button)
	{
		return !s_ImGuiHasMouseFocus && s_MouseButtons[static_cast<u16>(button)];
	}

	NODISCARD static FORCEINLINE bool IsMouseButtonDownThisFrame(const MouseButton button)
	{
		return !s_ImGuiHasMouseFocus && s_MouseButtons[static_cast<u16>(button)] && !s_PreviousMouseButtons[static_cast<
			u16>(button)];
	}

	NODISCARD static FORCEINLINE bool IsMouseButtonUp(const MouseButton button)
	{
		return !s_ImGuiHasMouseFocus && !s_MouseButtons[static_cast<u16>(button)];
	}

	NODISCARD static FORCEINLINE bool IsMouseButtonUpThisFrame(const MouseButton button)
	{
		return !s_ImGuiHasMouseFocus && !s_MouseButtons[static_cast<u16>(button)] && s_PreviousMouseButtons[static_cast<
			u16>(button)];
	}

	NODISCARD static FORCEINLINE glm::vec2 GetMousePosition() { return s_MousePosition; }
	NODISCARD static FORCEINLINE glm::vec2 GetMouseDelta() { return s_MouseDelta; }

	FORCEINLINE static void PreUpdate()
	{
		// Can't use memcpy_s here because it's not available on all compilers :(
		memcpy(s_PreviousKeys, s_Keys, static_cast<u16>(Scancode::Count) * sizeof(bool));
		memcpy(s_PreviousMouseButtons, s_MouseButtons, static_cast<u16>(MouseButton::Count) * sizeof(bool));
		s_MouseDelta = glm::vec2(0.0f);
#ifndef VULC_NO_IMGUI
		ImGuiIO& io             = ImGui::GetIO();
		s_ImGuiHasKeyboardFocus = io.WantCaptureKeyboard;
		s_ImGuiHasMouseFocus    = io.WantCaptureMouse;
#endif
	}

	static void ProcessKeyboardInputEvent(const SDL_KeyboardEvent& event);
	static void ProcessMouseInputEvent(const SDL_MouseButtonEvent& event);
	static void ProcessMouseMotionEvent(const SDL_MouseMotionEvent& event);

protected:
	// Remember to update UpdateKeyArrays() and Init() if changing the size of these arrays from VULC_KEY_COUNT
	// These arrays have to be the same size!
	static bool      s_Keys[static_cast<u16>(Scancode::Count)];
	static bool      s_PreviousKeys[static_cast<u16>(Scancode::Count)];
	static bool      s_MouseButtons[static_cast<u16>(MouseButton::Count)];
	static bool      s_PreviousMouseButtons[static_cast<u16>(MouseButton::Count)];
	static glm::vec2 s_MousePosition, s_MouseDelta;

	static bool s_ImGuiHasKeyboardFocus;
	static bool s_ImGuiHasMouseFocus;
};
