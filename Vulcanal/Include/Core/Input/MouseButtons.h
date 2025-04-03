#pragma once

enum class MouseButton : u8
{
	None    = 0,
	Left    = 1,
	Middle  = 2,
	Right   = 3,
	Button4 = 4,
	Button5 = 5,
	Button6 = 6,
	Button7 = 7,
	Button8 = 8,
	Count   = Button8,
};

NODISCARD const char* MouseButtonToString(MouseButton button);
