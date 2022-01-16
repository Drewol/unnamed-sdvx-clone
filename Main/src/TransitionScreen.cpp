#include "stdafx.h"
#include "TransitionScreen.hpp"
#include "Application.hpp"
#include "Shared/Jobs.hpp"
#include "AsyncLoadable.hpp"
#include "Game.hpp"

class TransitionScreen_Impl : public TransitionScreen
{
	IAsyncLoadableApplicationTickable *m_tickableToLoad;
	Job m_loadingJob;
	Texture m_fromTexture;
	Mesh m_bgMesh;
	lua_State *m_lua = nullptr;
	lua_State *m_songlua = nullptr;
	Vector<DelegateHandle> m_lamdasToRemove;
	Vector<void *> m_handlesToRemove;

	enum Transition
	{
		In,
		Wait,
		Out,
		End
	};
	Transition m_transition = Transition::In;
	bool m_loadComplete = false;
	float m_transitionTimer;
	bool m_lastComplete = false;
	bool m_isGame = false;
	bool m_stopped = false;
	bool m_canCancel = true;
	bool m_initialized = false;

	//0 = normal, 1 = song
	bool m_legacy[2] = {false, false};
	int m_jacketImg = 0;

	void m_InitTransition(IAsyncLoadableApplicationTickable *next)
	{
		if (m_loadingJob && !m_loadingJob->IsFinished())
		{
			m_loadingJob->Terminate();
		}

		m_loadingJob = JobBase::CreateLambda([&]() {
			return DoLoad();
		});
		m_loadingJob->OnFinished.Add(this, &TransitionScreen_Impl::OnFinished);
		m_fromTexture = TextureRes::CreateFromFrameBuffer(g_gl, g_resolution);
		m_bgMesh = MeshGenerators::Quad(g_gl, Vector2(0, g_resolution.y), Vector2(g_resolution.x, -g_resolution.y));
		m_tickableToLoad = next;
		m_stopped = false;
		m_loadComplete = false;
		m_transitionTimer = 0.0f;
		m_lastComplete = false;
		m_transition = Transition::In;
	}

public:
	~TransitionScreen_Impl()
	{
		// In case of forced removal of this screen
		if (!m_loadingJob->IsFinished())
			m_loadingJob->Terminate();

		if (m_lua)
			g_application->DisposeLua(m_lua);

		if (m_songlua)
			g_application->DisposeLua(m_songlua);

		if (m_jacketImg)
			nvgDeleteImage(g_application->GetVGContext(), m_jacketImg);
	}

	void RemoveAllOnComplete(void *handle) override
	{
		m_handlesToRemove.Add(handle);
	}
	void RemoveOnComplete(DelegateHandle handle) override
	{
		m_lamdasToRemove.Add(handle);
	}

	void Tick(float deltaTime) override
	{
		m_transitionTimer += deltaTime;

		if (m_transition == Wait && m_lastComplete)
		{
			m_transition = Out;
		}
		if (m_transition == Out && m_tickableToLoad)
		{
			m_tickableToLoad->Tick(deltaTime);
		}
		else if (m_transition == End)
		{
			g_application->RemoveTickable(this, true);
			if (m_tickableToLoad)
			{
				Log("[Transition] Finished loading tickable", Logger::Severity::Info);
				g_application->AddTickable(m_tickableToLoad);
			}
		}
		m_lastComplete = m_loadComplete;
	}
	void OnSuspend() override
	{
		if (m_tickableToLoad && m_transition == End)
		{
			Log("transition tickable nulled", Logger::Severity::Debug);
			m_tickableToLoad = nullptr;
		}
	}

	bool Init() override
	{
		if (m_initialized)
			return true;

		auto validateLua = [&](lua_State *L) {
			lua_getglobal(L, "reset");
			bool valid = lua_isfunction(L, -1);
			lua_settop(L, 0);
			return valid;
		};

		m_songlua = g_application->LoadScript("songtransition", true);
		m_lua = g_application->LoadScript("transition", true);
		if (m_songlua != nullptr && !validateLua(m_songlua))
		{
			g_application->DisposeLua(m_songlua);
			m_songlua = nullptr;
			Log("Song transition lua has no reset function.", Logger::Severity::Warning);
			m_legacy[1] = true;
		}

		if (m_lua != nullptr && !validateLua(m_lua))
		{
			g_application->DisposeLua(m_lua);
			m_lua = nullptr;
			m_legacy[0] = true;
			Log("Transition lua has no reset function.", Logger::Severity::Warning);
		}

		m_loadingJob = JobBase::CreateLambda([&]() {
			return DoLoad();
		});
		m_loadingJob->OnFinished.Add(this, &TransitionScreen_Impl::OnFinished);
		m_initialized = true;
		return true;
	}

	void TransitionTo(IAsyncLoadableApplicationTickable *next, bool noCancel, IApplicationTickable* before) override
	{
		m_isGame = false;
		m_InitTransition(next);
		m_canCancel = !noCancel;
		if (m_legacy[0])
		{
			if (m_lua != nullptr)
			{
				g_application->DisposeLua(m_lua);
			}
			m_lua = g_application->LoadScript("transition", true);
		}
		else if (m_lua != nullptr)
		{
			lua_getglobal(m_lua, "reset");
			lua_pcall(m_lua, 0, 0, 0);
			lua_settop(m_lua, 0);
		}

		if (m_lua == nullptr)
		{
			g_jobSheduler->Queue(m_loadingJob);
		}

		g_application->AddTickable(this, before);
	}

	void TransitionTo(Game* next, IApplicationTickable* before) override
	{
		m_isGame = true;
		m_canCancel = true;
		m_InitTransition(next);

		if (m_legacy[1])
		{
			if (m_songlua != nullptr)
			{
				g_application->DisposeLua(m_songlua);
			}
			m_songlua = g_application->LoadScript("songtransition", true);
		}

		if (m_songlua)
		{
			ChartIndex* chart = next->GetChartIndex();

			if (chart)
			{
				String path = Path::RemoveLast(chart->path);

				if (m_jacketImg)
					nvgDeleteImage(g_application->GetVGContext(), m_jacketImg);
				m_jacketImg = nvgCreateImage(g_application->GetVGContext(), (path + Path::sep + chart->jacket_path).c_str(), 0);
			}

			auto pushStringToTable = [this](const char *name, String data) {
				lua_pushstring(m_songlua, name);
				lua_pushstring(m_songlua, *data);
				lua_settable(m_songlua, -3);
			};

			auto pushIntToTable = [this](const char *name, int data) {
				lua_pushstring(m_songlua, name);
				lua_pushnumber(m_songlua, data);
				lua_settable(m_songlua, -3);
			};

			lua_newtable(m_songlua);

			if(chart)
			{
				pushStringToTable("title", chart->title);
				pushStringToTable("artist", chart->artist);
				pushStringToTable("effector", chart->effector);
				pushStringToTable("illustrator", chart->illustrator);
				pushStringToTable("bpm", chart->bpm);
				pushIntToTable("level", chart->level);
				pushIntToTable("difficulty", chart->diff_index);
				pushIntToTable("jacket", m_jacketImg);
			}
			else
			{
				pushStringToTable("title", "");
				pushStringToTable("artist", "");
				pushStringToTable("effector", "");
				pushStringToTable("illustrator", "");
				pushStringToTable("bpm", "");
				pushIntToTable("level", 1);
				pushIntToTable("difficulty", 0);
				pushIntToTable("jacket", m_jacketImg);
			}

			lua_setglobal(m_songlua, "song");
			if (!m_legacy[1])
			{
				lua_getglobal(m_songlua, "reset");
				lua_pcall(m_songlua, 0, 0, 0);
				lua_settop(m_songlua, 0);
			}
		}
		else
		{
			g_jobSheduler->Queue(m_loadingJob);
		}
		g_application->AddTickable(this, before);
	}

	void Render(float deltaTime) override
	{
		lua_State *lua = m_lua;
		if (m_isGame)
		{
			lua = m_songlua;
		}

		if (lua == nullptr)
			return;

		auto rq = g_application->GetRenderQueueBase();
		if (m_transition == Out || m_transition == End  || m_tickableToLoad == nullptr)
		{
			if (m_tickableToLoad)
			{
				m_tickableToLoad->ForceRender(deltaTime);
			}
			else
			{
				Transform t;
				MaterialParameterSet params;
				params.SetParameter("mainTex", m_fromTexture);
				params.SetParameter("color", Vector4(1.0f));
				rq->Draw(t, m_bgMesh, g_application->GetGuiTexMaterial(), params);
				g_application->ForceRender();
			}

			g_application->ForceRender();

			//draw lua
			lua_getglobal(lua, "render_out");
			lua_pushnumber(lua, deltaTime);
			if (lua_pcall(lua, 1, 1, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				m_transition = End;
				assert(false);
			}
			else
			{
				if (lua_toboolean(lua, lua_gettop(lua)))
				{
					m_transition = End;
				}
				lua_pop(lua, 1);
			}
		}
		else
		{
			Transform t;
			MaterialParameterSet params;
			params.SetParameter("mainTex", m_fromTexture);
			params.SetParameter("color", Vector4(1.0f));

			rq->Draw(t, m_bgMesh, g_application->GetGuiTexMaterial(), params);
			g_application->ForceRender();

			//draw lua
			lua_getglobal(lua, "render");
			lua_pushnumber(lua, deltaTime);
			if (lua_pcall(lua, 1, 1, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				g_jobSheduler->Queue(m_loadingJob);
				m_transition = Wait;
				assert(false);
			}
			else
			{
				if (lua_toboolean(lua, lua_gettop(lua)))
				{
					if (m_transition == In)
					{
						g_jobSheduler->Queue(m_loadingJob);
						m_transition = Wait;
					}
				}
				lua_pop(lua, 1);
			}
		}
	}

	void OnFinished(Job& job)
	{
		// Finalize?
		IAsyncLoadable *loadable = dynamic_cast<IAsyncLoadable *>(m_tickableToLoad);
		if (job->IsSuccessfull())
		{
			if (loadable && !loadable->AsyncFinalize())
			{
				Log("[Transition] Failed to finalize loading of tickable", Logger::Severity::Error);
				delete m_tickableToLoad;
				m_tickableToLoad = nullptr;
			}
			if (m_tickableToLoad && !m_tickableToLoad->DoInit()) //if it isn't null and init fails
			{
				Log("[Transition] Failed to initialize tickable", Logger::Severity::Error);
				delete m_tickableToLoad;
				m_tickableToLoad = nullptr;
			}
		}
		else
		{
			Log("[Transition] Failed to load tickable", Logger::Severity::Error);
			delete m_tickableToLoad;
			m_tickableToLoad = nullptr;
		}

		OnLoadingComplete.Call(m_tickableToLoad);

		for (void *v : m_handlesToRemove)
		{
			OnLoadingComplete.RemoveAll(v);
		}
		for (DelegateHandle h : m_lamdasToRemove)
		{
			OnLoadingComplete.Remove(h);
		}
		m_lamdasToRemove.clear();
		m_handlesToRemove.clear();

		m_loadComplete = true;
		if ((!m_isGame && m_lua == nullptr) || (m_isGame && m_songlua == nullptr))
			m_transition = End;
		m_transitionTimer = 0.0f;
	}
	bool DoLoad()
	{
		if (!m_tickableToLoad)
			return false;
		IAsyncLoadable *loadable = dynamic_cast<IAsyncLoadable *>(m_tickableToLoad);
		if (loadable)
		{
			if (!loadable->AsyncLoad())
			{
				Log("[Transition] Failed to load tickable", Logger::Severity::Error);
				return false;
			}
		}
		else
		{
			if (!m_tickableToLoad->DoInit())
				return false;
		}
		return true;
	}

	void OnKeyPressed(SDL_Scancode code, int32 delta) override
	{
		if (code == SDL_SCANCODE_ESCAPE && !m_stopped && m_canCancel)
		{
			m_stopped = true;
			if (m_loadingJob->IsQueued())
				m_loadingJob->Terminate();
			if (m_tickableToLoad)
				delete m_tickableToLoad;
			m_tickableToLoad = nullptr;
			g_application->RemoveTickable(this, true);
			OnLoadingComplete.Call(m_tickableToLoad);

			for (void *v : m_handlesToRemove)
			{
				OnLoadingComplete.RemoveAll(v);
			}
			for (DelegateHandle h : m_lamdasToRemove)
			{
				OnLoadingComplete.Remove(h);
			}
			m_lamdasToRemove.clear();
			m_handlesToRemove.clear();
		}
	}
};

TransitionScreen *TransitionScreen::Create()
{
	auto transition = new TransitionScreen_Impl();
	if (!transition->Init())
	{
		delete transition;
		transition = nullptr;
	}
	return transition;
}
