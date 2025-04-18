﻿#pragma once

// Our basic assertion macro - based on SDL_assert, but includes message formatting.
// see SDL_assert.h for more details for original implementation.

// TODO @mw: Assertions are currently not thread safe, and recursive assertions are not safely handled (we should abort if an assert causes an assert!).

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>

enum class AssertState : uint8_t
{
	AlwaysIgnore,
	Ignore,
	Silence,
	Retry,
	Break,
	Abort,
};

// Based on SDL_AssertData from SDL_assert.h.
struct AssertionData
{
	bool        AlwaysIgnored;
	bool        Silent;
	uint16_t    TriggerCount;
	int32_t     LineNumber;
	const char* Condition;
	const char* Filename;
	const char* Function;

	// We keep an internal linked list of assertion datas, so we can generate an assertion report.
	AssertionData* Next;
};

inline AssertionData* g_AssertionDataList = nullptr;

// The __FILE__ macro gives us the full path of the source file, which is useful but a bit verbose!
// This constexpr function will advance the pointer to just after the last slash character, so it's only the filename.
// I think it's prettier and easier to parse, and it's all at compile time, so free(ish)!
constexpr auto FullPathToFileName(const char* file)
{
	const char* pen       = file;
	const char* lastSlash = nullptr;

	while (*pen != '\0')
	{
		if (*pen == '/' || *pen == '\\')
			lastSlash = pen;
		pen++;
	}

	return lastSlash ? lastSlash + 1 : file;
}

inline std::string WrapString(const char* str, const int maxLineLength = 150, const bool onlyBreakOnSpaces = true)
{
	std::stringstream ss;
	int charsSinceLastNewline = 0;
	for (const char* p = str; *p != '\0'; p++)
	{
		if (*p == '\n')
		{
			ss << '\n';
			charsSinceLastNewline = 0;
			continue;
		}

		if (charsSinceLastNewline >= maxLineLength)
		{
			if (!onlyBreakOnSpaces || *p == ' ')
			{
				ss << '\n';
				charsSinceLastNewline = 0;

				if (* p == ' ')
				{
					// If we just broke on a space, skip it.
					p++;
					if (*p == '\0')
						break;
				}
			}
		}

		ss << *p;
		charsSinceLastNewline++;
	}
	return ss.str();
}

// This function will return the SDL window for the current application, if it exists.
// We define this in a separate CPP file, as otherwise we would have a circular dependency.
SDL_Window* GetAppWindow();

// This function does all the heavy lifting for assertions.
// It's pretty heavy, with lots of string operations and formatting, but we shouldn't be doing this often!
template <typename... Args>
AssertState ReportAssertion(AssertionData* data, const char* message = "Assertion Failed", Args&&... args)
{
	// Okay, we've asserted! Now what?
	// Firstly, is this assertion always ignored?
	// TODO @mw: We can probably remove this, since the macro itself checks.
	if (data->AlwaysIgnored)
		return AssertState::AlwaysIgnore;

	// Okay, we care. Firstly, let's update our assertion data.
	data->TriggerCount++;
	// Update our assertion data list.
	if (data->Next == nullptr)
	{
		// If next is nullptr, we know this assertion is either not in the list, or is the last one in the list.

		if (g_AssertionDataList == nullptr)
		{
			// If the list is empty, we can just set it to this assertion data.
			g_AssertionDataList = data;
		}
		else
		{
			// Otherwise, we need to add it to the end of the list.
			AssertionData* current = g_AssertionDataList;
			while (current->Next != nullptr)
				current = current->Next;
			if (current != data)
			{
				current->Next = data;
				data->Next    = nullptr;
			}
		}
	}

	// We'll format our messages using formatting functions.
	std::string formattedMessage = fmt::vformat(message, fmt::make_format_args(std::forward<Args>(args)...));
	std::string fullMessage      = fmt::format("Assertion failed: \"{}\" in {} at {}:{}\n\n{}\n", data->Condition,
	                                      data->Function, data->Filename, data->LineNumber, formattedMessage);
	fullMessage = WrapString(fullMessage.c_str());

	// Easy part first: print the message to the console. We don't use the macro, as we want to ensure the logger is non-null.
	if (g_VulcanalLogger)
		g_VulcanalLogger->error("{}" "\n", fullMessage);

	// If we're a silent assertion, we don't want to show a message box.
	if (data->Silent)
		return AssertState::Silence;
	
	// Now, let's construct a message box.
	SDL_MessageBoxData messageBoxData;
	messageBoxData.flags   = SDL_MESSAGEBOX_ERROR;
	messageBoxData.title   = "Assertion failed!";
	messageBoxData.message = fullMessage.c_str();
	messageBoxData.window  = GetAppWindow();

	// We have 5 buttons: Ignore, Always Ignore, Retry, Break, and Abort.
	// Pressing escape will retry, and pressing enter will break.
	SDL_MessageBoxButtonData buttonData[6];
	messageBoxData.buttons    = buttonData;
	messageBoxData.numbuttons = 6;
	buttonData[0].buttonID    = 1;
	buttonData[0].text        = "Ignore";
	buttonData[1].buttonID    = 2;
	buttonData[1].text        = "Always Ignore";
	buttonData[2].buttonID    = 6;
	buttonData[2].text        = "Silence";
	buttonData[3].buttonID    = 3;
	buttonData[3].text        = "Retry";
	buttonData[3].flags       = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
	buttonData[4].buttonID    = 4;
	buttonData[4].text        = "Break";
	buttonData[4].flags       = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
	buttonData[5].buttonID    = 5;
	buttonData[5].text        = "Abort";

	// Dark message boxes! God, I love living in the future. It's linux and android exclusive, sadly!
	SDL_MessageBoxColorScheme colorScheme;
	colorScheme.colors[SDL_MESSAGEBOX_COLOR_BACKGROUND].r = 50;
	colorScheme.colors[SDL_MESSAGEBOX_COLOR_BACKGROUND].g = 50;
	colorScheme.colors[SDL_MESSAGEBOX_COLOR_BACKGROUND].b = 50;
	colorScheme.colors[SDL_MESSAGEBOX_COLOR_TEXT].r       = 200;
	colorScheme.colors[SDL_MESSAGEBOX_COLOR_TEXT].g       = 200;
	colorScheme.colors[SDL_MESSAGEBOX_COLOR_TEXT].b       = 200;
	messageBoxData.colorScheme                            = &colorScheme;

	// Setup complete; lets show the message box we worked so hard on.
	int  buttonPressed = -1;
	bool success       = SDL_ShowMessageBox(&messageBoxData, &buttonPressed);
	if (!success)
	{
		VULC_ERROR("Couldn't even show a message box: {}\nWe're truly fucked; breaking.", SDL_GetError());
		buttonPressed = 4; // Abort
	}

	// If we didn't get a button pressed, we'll just break.
	if (buttonPressed == -1)
		buttonPressed = 4;

	// Alright, let's handle the button presses.
	switch (buttonPressed)
	{
	case 1:
		return AssertState::Ignore;
	case 2:
		data->AlwaysIgnored = true;
		return AssertState::AlwaysIgnore;
	case 3:
		return AssertState::Retry;
	case 4:
		// The macro will handle the breakpoint, so we break in the right place.
		return AssertState::Break;
	case 5:
		// ABORT!!!!!
		SDL_Quit(); // SDL calls this, so we will too. Sam knows best.
		std::abort();
	case 6:
		data->Silent = true;
		return AssertState::Silence;
	default:
		// Well this is awkward. We could assert here, but that would be a bit silly.
		VULC_WARN("Invalid assertion button pressed: {}. How'd you manage that?", buttonPressed);
		return AssertState::Break;
	}
}

inline void PrintAssertionReport()
{
	AssertionData* current = g_AssertionDataList;

	if (current == nullptr)
	{
		VULC_INFO("No assertions triggered.");
		return;
	}

	VULC_INFO("Assertions:");
	while (current != nullptr)
	{
		VULC_INFO("\"{}\" ({} at {}:{}) - Triggered {} times", current->Condition, current->Function,
		          current->Filename, current->LineNumber, current->TriggerCount);
		current = current->Next;
	}
}

// From SDL_assert.h:
#if defined(_MSC_VER)  /* Avoid /W4 warnings. */
/* "while (0,0)" fools Microsoft's compiler's /W4 warning level into thinking
	this condition isn't constant. And looks like an owl's face! */
#define NULL_WHILE_LOOP_CONDITION (0,0)
#else
#define NULL_WHILE_LOOP_CONDITION (0)
#endif

#define VULC_ENABLED_ASSERT(cond, silent, ...) \
	do { \
		static AssertionData data = { false, silent, 0, __LINE__, #cond, FullPathToFileName(__FILE__), __FUNCTION__, nullptr };\
		while (!data.AlwaysIgnored && !(cond))\
		{\
			const AssertState state = ReportAssertion(&data __VA_OPT__(,) ##__VA_ARGS__);\
			\
			if (state == AssertState::Retry) {\
				continue; /* We'll retry the condition. Do you know the definition of insanity? */ \
			} else if (state == AssertState::Break) {\
				SDL_TriggerBreakpoint(); /* If we want to break, we'll use SDLs macro for this, as it implements breaking on many platforms. */ \
			}\
			break; /* Otherwise, let's break out of our loop. */ \
		}\
	} while (NULL_WHILE_LOOP_CONDITION)

#define VULC_DISABLED_ASSERT(cond, ...) \
	do { \
		(void) sizeof((cond)); \
	} while (NULL_WHILE_LOOP_CONDITION)

// Checks are assertions that are always enabled, regardless of the build type.
#define VULC_CHECK(cond, ...) \
	VULC_ENABLED_ASSERT(cond, false, ##__VA_ARGS__)
#define VULC_CHECK_SILENT(cond, ...) \
	VULC_ENABLED_ASSERT(cond, true, ##__VA_ARGS__)

#ifndef VULC_DISABLE_ASSERTS
#define VULC_ASSERT(cond, ...) \
	VULC_ENABLED_ASSERT(cond, false, ##__VA_ARGS__)
#define VULC_ASSERT_SILENT(cond, ...) \
	VULC_ENABLED_ASSERT(cond, true, ##__VA_ARGS__)
#else
#define VULC_ASSERT(cond, ...) \
	VULC_DISABLED_ASSERT(cond, ##__VA_ARGS__)
#define VULC_ASSERT_SILENT(cond, ...) \
	VULC_DISABLED_ASSERT(cond, ##__VA_ARGS__)
#endif
