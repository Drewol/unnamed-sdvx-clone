#include "stdafx.h"
#include "Background.hpp"
#include "Application.hpp"
#include "GameConfig.hpp"
#include "Background.hpp"
#include "Game.hpp"
#include "Track.hpp"
#include "Camera.hpp"
#include "lua.hpp"

/* Background template for fullscreen effects */
class FullscreenBackground : public Background
{
public:
	virtual bool Init(bool foreground) override
	{
		fullscreenMesh = MeshGenerators::Quad(g_gl, Vector2(-1.0f), Vector2(2.0f));
		this->foreground = foreground;
		CheckedLoad(m_lua = g_application->LoadScript("game/background"));
		return true;
	}
	void UpdateRenderState(float deltaTime)
	{
		renderState = g_application->GetRenderStateBase();
	}
	virtual void Render(float deltaTime) override
	{
		assert(fullscreenMaterial);

		// Render a fullscreen quad
		RenderQueue rq(g_gl, renderState);
		rq.Draw(Transform(), fullscreenMesh, fullscreenMaterial, fullscreenMaterialParams);
		rq.Process();
	}

	RenderState renderState;
	Mesh fullscreenMesh;
	Material fullscreenMaterial;
	Texture backgroundTexture;
	Texture frameBufferTexture;
	MaterialParameterSet fullscreenMaterialParams;
	lua_State* m_lua = nullptr;
	float clearTransition = 0.0f;
	float offsyncTimer = 0.0f;
	bool foreground;
	bool hasFbTex;
};

class TestBackground : public FullscreenBackground
{
	virtual bool Init(bool foreground) override
	{
		using namespace std;
		if (!FullscreenBackground::Init(foreground))
			return false;

		/// TODO: Handle invalid background configurations properly
		/// e.g. missing bg_texture.png and such
		String skin = g_gameConfig.GetString(GameConfigKeys::Skin);
		String matPath = "";
		if (foreground)
		{
			frameBufferTexture = TextureRes::CreateFromFrameBuffer(g_gl, g_resolution);
			matPath = game->GetBeatmap()->GetMapSettings().foregroundPath;
			if (matPath.length() > 3 && matPath.substr(matPath.length() - 3, 3) == ".fs")
			{
				matPath = Path::Normalize(game->GetMapRootPath() + Path::sep + matPath);
				String texPath = Path::Normalize(game->GetMapRootPath() + Path::sep + "fg_texture.png");
				CheckedLoad(backgroundTexture = g_application->LoadTexture(texPath, true));
			}
			else
			{
				auto materialPaths = GetMaterialPaths(m_lua, true, matPath);
				CheckedLoad(backgroundTexture = g_application->LoadTexture(materialPaths.texture));
				matPath = "skins/" + skin + "/shaders/" + materialPaths.shader;
			}
		}
		else
		{
			matPath = game->GetBeatmap()->GetMapSettings().backgroundPath;
			if (matPath.length() > 3 && matPath.substr(matPath.length() - 3, 3) == ".fs")
			{
				matPath = Path::Normalize(game->GetMapRootPath() + Path::sep + matPath);
				String texPath = Path::Normalize(game->GetMapRootPath() + Path::sep + "bg_texture.png");
				CheckedLoad(backgroundTexture = g_application->LoadTexture(texPath, true));
			}
			else
			{
				auto materialPaths = GetMaterialPaths(m_lua, false, matPath);
				CheckedLoad(backgroundTexture = g_application->LoadTexture(materialPaths.texture));
				matPath = "skins/" + skin + "/shaders/" + materialPaths.shader;
			}
		}


		CheckedLoad(fullscreenMaterial = LoadBackgroundMaterial(matPath));
		fullscreenMaterial->opaque = false;
		hasFbTex = fullscreenMaterial->HasUniform("fb_tex");
		return true;
	}
	virtual void Render(float deltaTime) override
	{
		UpdateRenderState(deltaTime);

		Vector3 timing;
		const TimingPoint& tp = game->GetPlayback().GetCurrentTimingPoint();
		timing.x = game->GetPlayback().GetBeatTime();
		timing.z = game->GetPlayback().GetLastTime() / 1000.0f;
		offsyncTimer += (deltaTime / tp.beatDuration) * 1000.0 * game->GetPlaybackSpeed();
		offsyncTimer = fmodf(offsyncTimer, 1.0f);
		timing.y = offsyncTimer;

		float clearBorder = 0.70f;
		if ((game->GetFlags() & GameFlags::Hard) != GameFlags::None)
		{
			clearBorder = 0.30f;
		}

		bool cleared = game->GetScoring().currentGauge >= clearBorder;

		if (cleared)
			clearTransition += deltaTime / tp.beatDuration * 1000;
		else
			clearTransition -= deltaTime / tp.beatDuration * 1000;

		clearTransition = Math::Clamp(clearTransition, 0.0f, 1.0f);


		Vector3 trackEndWorld = Vector3(0.0f, 25.0f, 0.0f);
		Vector2i screenCenter = game->GetCamera().GetScreenCenter();



		float tilt = game->GetCamera().GetActualRoll() + game->GetCamera().GetBackgroundSpin();
		fullscreenMaterialParams.SetParameter("clearTransition", clearTransition);
		fullscreenMaterialParams.SetParameter("tilt", tilt);
		fullscreenMaterialParams.SetParameter("mainTex", backgroundTexture);
		fullscreenMaterialParams.SetParameter("screenCenter", screenCenter);
		fullscreenMaterialParams.SetParameter("timing", timing);
		if (foreground && hasFbTex)
		{
			frameBufferTexture->SetFromFrameBuffer();
			fullscreenMaterialParams.SetParameter("fb_tex", frameBufferTexture);
		}
		if (foreground)
		{
			float width = backgroundTexture->GetSize().x;
			fullscreenMaterialParams.SetParameter("framesCount", floor(width / 600));
		}

		FullscreenBackground::Render(deltaTime);
	}


	Material LoadBackgroundMaterial(const String& path)
	{
		String skin = g_gameConfig.GetString(GameConfigKeys::Skin);
		String pathV = Path::Absolute(String("skins/" + skin + "/shaders/") + "background" + ".vs");
		String pathF = Path::Absolute(path);
		String pathG = Path::Absolute(String("skins/" + skin + "/shaders/") + "background" + ".gs");
		Material ret = MaterialRes::Create(g_gl, pathV, pathF);
		// Additionally load geometry shader
		if (Path::FileExists(pathG))
		{
			Shader gshader = ShaderRes::Create(g_gl, ShaderType::Geometry, pathG);
			assert(gshader);
			ret->AssignShader(ShaderType::Geometry, gshader);
		}
		assert(ret);
		return ret;
	}

	Texture LoadBackgroundTexture(const String& path)
	{
		Texture ret = TextureRes::Create(g_gl, ImageRes::Create(path));
		return ret;
	}

	MaterialPaths GetMaterialPaths(lua_State* m_lua, bool foreground, String textureName)
	{
		char* textureGetCommand = foreground ? "get_fg_file"    : "get_bg_file";
		String texture          = foreground ? "fg_texture.png" : "bg_texture.png";
		String shader           = foreground ? "foreground.fs"  : "background.fs";
		
		lua_getglobal(m_lua, textureGetCommand);
		if (lua_isfunction(m_lua, -1))
		{
			lua_pushstring(m_lua, *textureName);
			if (lua_pcall(m_lua, 1, 2, 0) != 0)
			{
				Logf("Lua error on %s: %s", Logger::Error, textureGetCommand, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error on " + String(textureGetCommand), lua_tostring(m_lua, -1), 0);
			}
			else if (lua_isstring(m_lua, -2) && lua_isstring(m_lua, -1))
			{
				texture = lua_tostring(m_lua, -2);
				shader = lua_tostring(m_lua, -1);
			}
		}
		lua_settop(m_lua, 0);
		return MaterialPaths{texture, shader};
	}
};



Background* CreateBackground(class Game* game, bool foreground /* = false*/)
{
	Background* bg = new TestBackground();
	bg->game = game;
	if (!bg->Init(foreground))
	{
		delete bg;
		return nullptr;
	}
	return bg;
}
