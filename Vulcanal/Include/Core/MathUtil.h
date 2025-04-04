#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

template <typename T = float>
struct TRect
{
	glm::vec<2, T> Position;
	glm::vec<2, T> Size;

	TRect(glm::vec<2, T> position, glm::vec<2, T> size)
		: Position(position), Size(size)
	{
	}

	TRect(T x, T y, T width, T height)
		: Position(glm::vec<2, T>(x, y)), Size(glm::vec<2, T>(width, height))
	{
	}

	bool OverlapsWith(const TRect& other) const
	{
		return Position.x < other.Position.x + other.Size.x && // Not to the right of the other rectangle
			Position.x + Size.x > other.Position.x &&          // Not to the left of the other rectangle
			Position.y < other.Position.y + other.Size.y &&    // Not below the other rectangle
			Position.y + Size.y > other.Position.y;            // Not above the other rectangle
	}

	bool OverlapsWith(T x, T y, T width, T height) const
	{
		return Position.x < x + width && // Not to the right of the other rectangle
			Position.x + Size.x > x &&   // Not to the left of the other rectangle
			Position.y < y + height &&   // Not below the other rectangle
			Position.y + Size.y > y;     // Not above the other rectangle
	}

	bool ContainsRect(const TRect& other) const
	{
		return Position.x <= other.Position.x && Position.x + Size.x >= other.Position.x + other.Size.x
			&& Position.y <= other.Position.y && Position.y + Size.y >= other.Position.y + other.Size.y;
	}

	bool ContainsPoint(const glm::vec2& point) const
	{
		return Position.x <= point.x && Position.x + Size.x >= point.x
			&& Position.y <= point.y && Position.y + Size.y >= point.y;
	}

	glm::vec<2, T> GetCenter() const
	{
		return Position + (Size / 2.0f);
	}
};

using FRect = TRect<float>;
using DRect = TRect<double>;
using IRect = TRect<int>;

class MathUtil
{
public:
	static glm::mat4 CreateTransformationMatrix(const glm::vec3& translation, const glm::vec3& rotation,
		const glm::vec3& scale)
	{
		auto translationMatrix = translate(glm::mat4(1.0f), translation);
		auto rotationMatrix = glm::yawPitchRoll(glm::radians(rotation.y), glm::radians(rotation.x), glm::radians(rotation.z));
		auto scaleMatrix = glm::scale(glm::mat4(1.0), scale);
		return translationMatrix * rotationMatrix * scaleMatrix;
	}

	static float LerpSmooth(float a, float b, float r, float delta)
	{
		return (a - b) * glm::pow(r, delta) + b;
	}

	static glm::vec2 LerpSmooth(glm::vec2 a, glm::vec2 b, float r, float delta)
	{
		return { LerpSmooth(a.x, b.x, r, delta), LerpSmooth(a.y, b.y, r, delta) };
	}

	static glm::vec3 LerpSmooth(const glm::vec3& a, const glm::vec3& b, float r, float delta)
	{
		return { LerpSmooth(a.x, b.x, r, delta), LerpSmooth(a.y, b.y, r, delta), LerpSmooth(a.z, b.z, r, delta) };
	}

	static float Lerp(float a, float b, float t)
	{
		return a + ((b - a) * t);
	}

	static glm::vec3 Vec3Lerp(glm::vec3 a, glm::vec3 b, float t)
	{
		return glm::vec3(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t));
	}

	static glm::vec3 HSVToRGB(glm::vec3 hsv)
	{
		glm::vec4 K = glm::vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
		glm::vec3 p = glm::abs(glm::fract(glm::vec3(hsv.x) + glm::vec3(K.x, K.y, K.z)) * 6.0f - glm::vec3(K.w));
		return hsv.z * Vec3Lerp(glm::vec3(K.x), glm::clamp(p - glm::vec3(K.x), 0.0f, 1.0f), hsv.y);
	}

	static uint32_t Vec4ToAGBRInt(glm::vec4 colour)
	{
		glm::ivec4 bytes(colour.x * 255, colour.y * 255, colour.z * 255, colour.w * 255);
		uint32_t result = (bytes.r) | (bytes.g << 8) | (bytes.b << 16) | (bytes.a << 24);
		return result;
	}
};
