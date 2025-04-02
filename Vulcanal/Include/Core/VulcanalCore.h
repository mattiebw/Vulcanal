#pragma once

// I usually go for PascalCase for types, but I want lowercase for the number typedefs, so we'll just disable this inspection in this file.
// ReSharper disable CppInconsistentNaming

#define BIT(x) (1 << (x))

// MSVC compiler intrinsics
// MW @todo: Add intrinsics for other compilers (gcc/clang)
#ifdef VULC_PLATFORM_WINDOWS
	#define UNALIGNED __unaligned
	#define FORCEINLINE __forceinline
#else
	#define UNALIGNED
	#define FORCEINLINE inline
#endif

#ifdef VULC_DEBUG
	#define FORCEINLINE_DEBUGGABLE 
#else // ifdef VULC_DEBUG
	#define FORCEINLINE_DEBUGGABLE FORCEINLINE
#endif // elifdef VULC_DEBUG

#if __cplusplus >= 201703L or (defined(_MSVC_LANG) and _MSVC_LANG >= 201703L)
	#define NODISCARD [[nodiscard]]
#else
	#define NODISCARD
#endif

// -----------------------------------------------------------------------------------------------
// Array size macro
// From winnt.h:
//
// RtlpNumberOf is a function that takes a reference to an array of N Ts.
//
// typedef T array_of_T[N];
// typedef array_of_T &reference_to_array_of_T;
//
// RtlpNumberOf returns a pointer to an array of N chars.
// We could return a reference instead of a pointer but older compilers do not accept that.
//
// typedef char array_of_char[N];
// typedef array_of_char *pointer_to_array_of_char;
//
// sizeof(array_of_char) == N
// sizeof(*pointer_to_array_of_char) == N
//
// pointer_to_array_of_char RtlpNumberOf(reference_to_array_of_T);
//
// We never even call RtlpNumberOf, we just take the size of dereferencing its return type.
// We do not even implement RtlpNumberOf, we just declare it.
//
// Attempts to pass pointers instead of arrays to this macro result in compile time errors.
// That is the point.
//
extern "C++" // templates cannot be declared to have 'C' linkage
template <typename T, size_t N>
char (* RtlpNumberOf(UNALIGNED T (&)[N]))[N];
#define VULC_ARRAY_SIZE(A) (sizeof(*RtlpNumberOf(A)))
// -----------------------------------------------------------------------------------------------

// Basic swap macro
#define VULC_SWAP(IntermediateType, a, b) do { IntermediateType t = a; (a) = b; (b) = t; } while (0)

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using i8  = int8_t; // Should we have both?
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

template <typename T>
using Scope = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr Scope<T> CreateScope(Args &&... args)
{
	return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T, typename... Args>
constexpr Ref<T> CreateRef(Args &&... args)
{
	return std::make_shared<T>(std::forward<Args>(args)...);
}

#define VULC_ASSERT SDL_assert

struct SemVer
{
	u16 Major;
	u16 Minor;
	u16 Patch;

	SemVer()
		: Major(0), Minor(0), Patch(0)
	{
	}

	explicit SemVer(const u16 major)
		: Major(major), Minor(0), Patch(0)
	{
	}

	SemVer(const u16 major, const u16 minor, const u16 patch)
		: Major(major), Minor(minor), Patch(patch)
	{
	}

	explicit SemVer(const u64 packed)
		: Major(static_cast<u16>((packed >> 48) & 0xffff)),
		  Minor(static_cast<u16>((packed >> 32) & 0xffff)),
		  Patch(static_cast<u16>(packed & 0xffff))
	{
	}

	// SemVer comparison operators
	bool operator==(const SemVer &other) const
	{
		return Major == other.Major && Minor == other.Minor && Patch == other.Patch;
	}

	bool operator!=(const SemVer &other) const
	{
		return !(*this == other);
	}

	bool operator<(const SemVer &other) const
	{
		if (Major < other.Major)
			return true;
		if (Major > other.Major)
			return false;

		if (Minor < other.Minor)
			return true;
		if (Minor > other.Minor)
			return false;

		return Patch < other.Patch;
	}

	NODISCARD u64 Packed() const
	{
		return (static_cast<u64>(Major) << 48) | (static_cast<u64>(Minor) << 32) | static_cast<u64>(Patch);
	}
};

// Generate CRC lookup table
template <unsigned c, int k = 8>
struct crcf : crcf<((c & 1) ? 0xedb88320 : 0) ^ (c >> 1), k - 1>
{
};

template <unsigned c>
struct crcf<c, 0>
{
	enum { value = c };
};

#define CRCA(x) CRCB(x) CRCB(x + 128)
#define CRCB(x) CRCc(x) CRCc(x +  64)
#define CRCc(x) CRCD(x) CRCD(x +  32)
#define CRCD(x) CRCE(x) CRCE(x +  16)
#define CRCE(x) CRCF(x) CRCF(x +   8)
#define CRCF(x) CRCG(x) CRCG(x +   4)
#define CRCG(x) CRCH(x) CRCH(x +   2)
#define CRCH(x) CRCI(x) CRCI(x +   1)
#define CRCI(x) crcf<x>::value ,

constexpr unsigned crc_table[] = {CRCA(0)}; // Rider doesn't like this, but it compiles fine!

constexpr u32 crc32(std::string_view str)
{
	u32 crc = 0xffffffff;
	for (auto c : str)
		crc = (crc >> 8) ^ crc_table[(crc ^ c) & 0xff];
	return crc ^ 0xffffffff;
}

constexpr u16 crc16(std::string_view str)
{
	u16 crc = 0xffff;
	for (auto c : str)
		crc = (crc >> 8) ^ crc_table[(crc ^ c) & 0xff];
	return crc ^ 0xffff;
}
