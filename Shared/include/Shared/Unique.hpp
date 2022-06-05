#pragma once
#ifndef UNIQUE_H
#define UNIQUE_H
/*
	Inherit from this to disallow copying
*/
class Unique
{
protected:
	Unique(const Unique& rhs) = delete;
	Unique& operator=(const Unique& rhs) = delete;
	Unique() = default;
};
#endif
