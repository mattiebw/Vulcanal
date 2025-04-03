#include "vulcpch.h"
#include "Core/Input/Input.h"

bool      Input::s_Keys[static_cast<u16>(Scancode::Count)];
bool      Input::s_PreviousKeys[static_cast<u16>(Scancode::Count)];
bool      Input::s_MouseButtons[static_cast<u16>(MouseButton::Count)];
bool      Input::s_PreviousMouseButtons[static_cast<u16>(MouseButton::Count)];
glm::vec2 Input::s_MousePosition, Input::s_MouseDelta;
bool      Input::s_ImGuiHasKeyboardFocus = false;
bool      Input::s_ImGuiHasMouseFocus    = false;

void Input::Init()
{
	memset(s_Keys, 0, static_cast<u16>(Scancode::Count) * sizeof(bool));
	memset(s_PreviousKeys, 0, static_cast<u16>(Scancode::Count) * sizeof(bool));
	memset(s_MouseButtons, 0, static_cast<u16>(MouseButton::Count) * sizeof(bool));
	memset(s_PreviousMouseButtons, 0, static_cast<u16>(MouseButton::Count) * sizeof(bool));
}

void Input::Shutdown()
{
	// Nothing to do here for now, but if we add static delegates or something, we might need to clean up.
}

void Input::ProcessKeyboardInputEvent(const SDL_KeyboardEvent &event)
{
	s_Keys[event.scancode] = event.down;
}

void Input::ProcessMouseInputEvent(const SDL_MouseButtonEvent &event)
{
	s_MouseButtons[event.button] = event.down;
}

void Input::ProcessMouseMotionEvent(const SDL_MouseMotionEvent &event)
{
	s_MousePosition = {event.x, event.y};
	s_MouseDelta    = {event.xrel, event.yrel};
}

const char* MouseButtonToString(MouseButton button)
{
	switch (button)
	{
	case MouseButton::None:
		return "None";
	case MouseButton::Left:
		return "Left Mouse Button";
	case MouseButton::Middle:
		return "Middle Mouse Button";
	case MouseButton::Right:
		return "Right Mouse Button";
	case MouseButton::Button4:
		return "Mouse Button 4";
	case MouseButton::Button5:
		return "Mouse Button 5";
	case MouseButton::Button6:
		return "Mouse Button 6";
	case MouseButton::Button7:
		return "Mouse Button 7";
	case MouseButton::Button8:
		return "Mouse Button 8";
	default:
		VULC_ERROR("Invalid mouse button ({0}) in MouseButtonToString()", static_cast<int>(button));
		return "Unknown Mouse Button";
	}
}

const char* ScancodeToString(MouseButton button)
{
	return SDL_GetScancodeName(static_cast<SDL_Scancode>(button));
}
