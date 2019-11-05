#pragma once

/*
	Inherit from this to disallow copying
*/
class NonCopyable
{
protected:
	NonCopyable(const NonCopyable& rhs) = delete;
	NonCopyable& operator=(const NonCopyable& rhs) = delete;
	NonCopyable() = default;
};
