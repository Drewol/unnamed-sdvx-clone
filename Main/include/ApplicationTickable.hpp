#pragma once

class IApplicationTickable
{
public:
	virtual ~IApplicationTickable() = default;
	// Run to call initialization code
	virtual bool DoInit();
	// Override for initialization
	virtual bool Init() { return true; }
	// Tick for tickable
	virtual void Tick(float deltaTime) {};
	virtual void Render(float deltaTime) {};
	virtual void ForceRender(float deltaTime) { Render(deltaTime); };
	virtual void OnKeyPressed(SDL_Scancode code, int32 delta) {};
	virtual void OnKeyReleased(SDL_Scancode code, int32 delta) {};
	// Called when focus of this item is lost
	virtual void OnSuspend() {};
	// Called when focus to this item is restored
	virtual void OnRestore() {};

	// Return true to override application ticking behaviour
	virtual bool GetTickRate(int32& rate) { return false; };

	bool IsSuspended() const { return m_suspended; }
	bool IsSuccessfullyInitialized() const { return m_successfullyInitialized; }

private:
	void m_Suspend();
	void m_Restore();

	bool m_successfullyInitialized = false;
	bool m_initialized = false;
	bool m_suspended = true;
	friend class Application;
};
