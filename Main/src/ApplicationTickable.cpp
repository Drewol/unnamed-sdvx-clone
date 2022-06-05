#include "stdafx.h"
#include "ApplicationTickable.hpp"

bool IApplicationTickable::DoInit()
{
	if (!m_initialized)
	{
		m_successfullyInitialized = Init();
		m_initialized = true;
	}
	return m_successfullyInitialized;
}
void IApplicationTickable::m_Suspend()
{
	if (!m_suspended)
	{
		OnSuspend();
		m_suspended = true;
	}
}
void IApplicationTickable::m_Restore()
{
	if (m_suspended)
	{
		OnRestore();
		m_suspended = false;
	}
}
