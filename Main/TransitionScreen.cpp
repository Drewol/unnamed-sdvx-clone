#include "stdafx.h"
#include "TransitionScreen.hpp"
#include "Application.hpp"
#include "Shared/Jobs.hpp"
#include "AsyncLoadable.hpp"
#include "lua.hpp"
#include "Game.hpp"
#include "nanovg.h"

class TransitionScreen_Impl : public TransitionScreen
{
	IAsyncLoadableApplicationTickable* m_tickableToLoad;
	Job m_loadingJob;
	Texture m_fromTexture;
	Mesh m_bgMesh;
	lua_State* m_lua = nullptr;

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
	int m_jacketImg = 0;

public:
	TransitionScreen_Impl(IAsyncLoadableApplicationTickable* next, bool noCancel)
	{
		m_tickableToLoad = next;
		m_canCancel = !noCancel;
	}

	TransitionScreen_Impl(Game* next)
	{
		m_tickableToLoad = next;
		m_isGame = true;

	}

	~TransitionScreen_Impl()
	{
		// In case of forced removal of this screen
		if(!m_loadingJob->IsFinished())
			m_loadingJob->Terminate();

		if (m_lua)
			g_application->DisposeLua(m_lua);

		if (m_jacketImg)
			nvgDeleteImage(g_application->GetVGContext(), m_jacketImg);
		//g_rootCanvas->Remove(m_loadingOverlay.As<GUIElementBase>());
	}
	virtual void Tick(float deltaTime)
	{
		m_transitionTimer += deltaTime;
		
		if(m_transition == Wait && m_lastComplete)
		{
			m_transition = Out;
		}
		if (m_transition == Out && m_tickableToLoad)
		{
			m_tickableToLoad->Tick(deltaTime);
		}
		else if(m_transition == End)
		{
			g_application->RemoveTickable(this);
			if (m_tickableToLoad)
			{
				Log("[Transition] Finished loading tickable", Logger::Info);
				g_application->AddTickable(m_tickableToLoad, this);
			}
		}
		m_lastComplete = m_loadComplete;
	}
	virtual bool Init()
	{
		if(!m_tickableToLoad)
			return false;

		m_fromTexture = TextureRes::CreateFromFrameBuffer(g_gl, g_resolution);
		m_bgMesh = MeshGenerators::Quad(g_gl, Vector2(0, g_resolution.y), Vector2(g_resolution.x, -g_resolution.y));

		if (m_isGame)
		{
			m_lua = g_application->LoadScript("songtransition", true);
			if (m_lua)
			{
				Game* game = (Game*)m_tickableToLoad;
				String path = Path::RemoveLast(game->GetDifficultyIndex().path);
				auto& gameSettings = game->GetDifficultyIndex().settings;

				m_jacketImg = nvgCreateImage(g_application->GetVGContext(), (path + Path::sep + gameSettings.jacketPath).c_str(), 0);

				auto pushStringToTable = [this](const char* name, String data)
				{
					lua_pushstring(m_lua, name);
					lua_pushstring(m_lua, *data);
					lua_settable(m_lua, -3);
				};

				auto pushIntToTable = [this](const char* name, int data)
				{
					lua_pushstring(m_lua, name);
					lua_pushnumber(m_lua, data);
					lua_settable(m_lua, -3);
				};

				lua_newtable(m_lua);
				{
					pushStringToTable("title", gameSettings.title);
					pushStringToTable("artist", gameSettings.artist);
					pushStringToTable("effector", gameSettings.effector);
					pushStringToTable("illustrator", gameSettings.illustrator);
					pushStringToTable("bpm", gameSettings.bpm);
					pushIntToTable("level", gameSettings.level);
					pushIntToTable("difficulty", gameSettings.difficulty);
					pushIntToTable("jacket", m_jacketImg);
				}
				lua_setglobal(m_lua, "song");
			}
		}
		else
			m_lua = g_application->LoadScript("transition", true);


		m_loadingJob = JobBase::CreateLambda([&]()
		{
			return DoLoad();
		});
		m_loadingJob->OnFinished.Add(this, &TransitionScreen_Impl::OnFinished);
		if(m_lua == nullptr)
			g_jobSheduler->Queue(m_loadingJob);

		return true;
	}

	void Render(float deltaTime)
	{
		if (m_lua == nullptr)
			return;

		auto rq = g_application->GetRenderQueueBase();
		if (m_transition == Out || m_transition == End)
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
			lua_getglobal(m_lua, "render_out");
			lua_pushnumber(m_lua, deltaTime);
			if (lua_pcall(m_lua, 1, 1, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				g_application->DisposeLua(m_lua);
				m_lua = nullptr;
				m_transition = End;
				assert(false);
			}
			else
			{
				if (lua_toboolean(m_lua, lua_gettop(m_lua)))
				{
					m_transition = End;
				}
				lua_pop(m_lua, 1);
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
			lua_getglobal(m_lua, "render");
			lua_pushnumber(m_lua, deltaTime);
			if (lua_pcall(m_lua, 1, 1, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				g_application->DisposeLua(m_lua);
				g_jobSheduler->Queue(m_loadingJob);
				m_transition = Wait;
				m_lua = nullptr;
				assert(false);
			}
			else
			{
				if (lua_toboolean(m_lua, lua_gettop(m_lua)))
				{
					if (m_transition == In)
					{
						g_jobSheduler->Queue(m_loadingJob);
						m_transition = Wait;
					}
				}
				lua_pop(m_lua, 1);
			}
		}

	}

	void OnFinished(Job job)
	{
		// Finalize?
		IAsyncLoadable* loadable = dynamic_cast<IAsyncLoadable*>(m_tickableToLoad);
		if(job->IsSuccessfull())
		{
			if(loadable && !loadable->AsyncFinalize())
			{
				Log("[Transition] Failed to finalize loading of tickable", Logger::Error);
				delete m_tickableToLoad;
				m_tickableToLoad = nullptr;
			}
			if (m_tickableToLoad && !m_tickableToLoad->Init()) //if it isn't null and init fails
			{
				Log("[Transition] Failed to initialize tickable", Logger::Error);
				delete m_tickableToLoad;
				m_tickableToLoad = nullptr;
			}
		}
		else
		{
			Log("[Transition] Failed to load tickable", Logger::Error);
			delete m_tickableToLoad;
			m_tickableToLoad = nullptr;
		}

		OnLoadingComplete.Call(m_tickableToLoad);
		m_loadComplete = true;
		if (m_lua == nullptr)
			m_transition = End;
		m_transitionTimer = 0.0f;
	}
	bool DoLoad()
	{
		if(!m_tickableToLoad)
			return false;
		IAsyncLoadable* loadable = dynamic_cast<IAsyncLoadable*>(m_tickableToLoad);
		if(loadable)
		{
			if(!loadable->AsyncLoad())
			{
				Log("[Transition] Failed to load tickable", Logger::Error);
				return false;
			}
		}
		else
		{
			if(!m_tickableToLoad->DoInit())
				return false;
		}
		return true;
	}

	void OnKeyPressed(int32 key)
	{
		if (key == SDLK_ESCAPE && !m_stopped && m_canCancel)
		{
			m_stopped = true;
			if(m_loadingJob->IsQueued())
				m_loadingJob->Terminate();
			if (m_tickableToLoad)
				delete m_tickableToLoad;
			m_tickableToLoad = nullptr;
			g_application->RemoveTickable(this);
		}
	}
};

TransitionScreen* TransitionScreen::Create(IAsyncLoadableApplicationTickable* next, bool noCancel)
{
	return new TransitionScreen_Impl(next, noCancel);
}

TransitionScreen* TransitionScreen::Create(Game* game)
{
	return new TransitionScreen_Impl(game);
}
