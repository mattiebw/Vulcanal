#pragma once

template <>
struct fmt::formatter<glm::ivec2>
{
public:
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	template <typename Context>
	constexpr auto format(const glm::ivec2 &vec, Context &ctx) const
	{
		return fmt::format_to(ctx.out(), "({}, {})", vec.x, vec.y);
	}
};

template <>
struct fmt::formatter<glm::ivec3>
{
public:
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	template <typename Context>
	constexpr auto format(const glm::ivec3 &vec, Context &ctx) const
	{
		return fmt::format_to(ctx.out(), "({}, {}, {})", vec.x, vec.y, vec.z);
	}
};

template <>
struct fmt::formatter<glm::vec2>
{
public:
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	template <typename Context>
	constexpr auto format(const glm::vec2 &vec, Context &ctx) const
	{
		return fmt::format_to(ctx.out(), "({}, {})", vec.x, vec.y);
	}
};

template <>
struct fmt::formatter<glm::vec3>
{
public:
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	template <typename Context>
	constexpr auto format(const glm::vec3 &vec, Context &ctx) const
	{
		return fmt::format_to(ctx.out(), "({}, {}, {})", vec.x, vec.y, vec.z);
	}
};

template <>
struct fmt::formatter<glm::vec4>
{
public:
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	template <typename Context>
	constexpr auto format(const glm::vec4 &vec, Context &ctx) const
	{
		return fmt::format_to(ctx.out(), "({}, {}, {}, {})", vec.x, vec.y, vec.z, vec.w);
	}
};

template <>
struct fmt::formatter<SemVer>
{
public:
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	template <typename Context>
	constexpr auto format(const SemVer &ver, Context &ctx) const
	{
		return fmt::format_to(ctx.out(), "v{}.{}.{}", ver.Major, ver.Minor, ver.Patch);
	}
};
