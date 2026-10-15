#pragma once

namespace aZero
{
	class NonCopyable
	{
	public:
		NonCopyable() = default;
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
		NonCopyable(NonCopyable&&) noexcept = default;
		NonCopyable& operator=(NonCopyable&&) noexcept = default;
	};

	class NonMovable
	{
	public:
		NonMovable() = default;
		NonMovable(NonMovable&&) = delete;
		NonMovable& operator=(NonMovable&&) = delete;
	};
}