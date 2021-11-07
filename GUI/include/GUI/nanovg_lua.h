#pragma once
#include "nanovg.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "Graphics/Font.hpp"
#include "Graphics/RenderQueue.hpp"
#include "Shared/Transform.hpp"
#include "Shared/Files.hpp"
#include "Shared/Thread.hpp"
#include <atomic>
#include <mutex>

struct Label
{
	Text text;
	int size;
	float scale;
	FontRes::TextOptions opt;
	Graphics::Font font;
	String content;
};


struct ImageAnimation
{
	int FrameCount;
	std::atomic<int32_t> CurrentFrame;
	int TimesToLoop;
	int LoopCounter;
	int w;
	int h;
	float SecondsPerFrame;
	float Timer;
	std::atomic<bool> Compressed;
	std::atomic<bool> LoadComplete;
	std::atomic<bool> Cancelled;
	std::mutex LoadMutex;
	Vector<Graphics::Image> Frames;
	Vector<Buffer> FrameData; //for storing the file contents of the compressed frames
	Image CurrentImage; //the current uncompressed frame in use
	Image NextImage;
	Thread* JobThread;
	lua_State* State;
};

struct GUIState
{
	NVGcontext* vg;
	RenderQueue* rq;
	Transform t;
	Map<lua_State*, Map<int, Label>> textCache;
	Map<lua_State*, Map<int, NVGpaint>> paintCache;
	Map<lua_State*, int> nextTextId;
	Map<lua_State*, int> nextPaintId;
	Map<String, Graphics::Font> fontCahce;
	Map<lua_State*, Set<int>> vgImages;
	Graphics::Font currentFont;
	Vector4 fillColor;
	int textAlign;
	int fontSize;
	Material* fontMaterial;
	Material* fillMaterial;
	NVGcolor otrColor; //outer color
	NVGcolor inrColor; //inner color
	NVGcolor imageTint;
	Rect scissor;
	Vector2i resolution;
	Map<int, Ref<ImageAnimation>> animations;
	int scissorOffset;
	Vector<Transform> transformStack;
	Vector<int> nvgFonts;
};




GUIState g_guiState;

static int LoadFont(const char* name, const char* filename, lua_State* L)
{
	{
		Graphics::Font* cached = g_guiState.fontCahce.Find(name);
		if (cached)
		{
			g_guiState.currentFont = *cached;
		}
		else
		{
			String path = filename;
			Graphics::Font newFont = FontRes::Create(g_gl, path);
			if (!newFont)
			{
				lua_Debug ar;
				lua_getstack(L, 1, &ar);
				lua_getinfo(L, "Snl", &ar);
				String luaFilename;
				String fontFilename;
				Path::RemoveLast(filename, &fontFilename);
				Path::RemoveLast(ar.source, &luaFilename);
				lua_pushstring(L, *Utility::Sprintf("Failed to load font \"%s\" at line %d in \"%s\"", fontFilename, ar.currentline, luaFilename));
				lua_error(L);
				return 0;
			}
			g_guiState.fontCahce.Add(name, newFont);
			g_guiState.currentFont = *g_guiState.fontCahce.Find(name);
		}
	}

	{ //nanovg
		if (nvgFindFont(g_guiState.vg, name) != -1)
		{
			nvgFontFace(g_guiState.vg, name);
			return 0;
		}
		int fontId = nvgCreateFont(g_guiState.vg, name, filename);
		nvgFontFaceId(g_guiState.vg, fontId);
		nvgAddFallbackFont(g_guiState.vg, name, "fallback");
	}
	return 0;
}

static int lBeginPath(lua_State* L)
{
	g_guiState.fillColor = Vector4(1.0);
	nvgBeginPath(g_guiState.vg);
	return 0;
}


static void AnimationLoader(Vector<FileInfo> files, Ref<ImageAnimation> ia)
{
	ia->FrameCount = files.size();
	files.Sort([](FileInfo& a, FileInfo& b) {
		String af, bf;
		Path::RemoveLast(a.fullPath, &af);
		Path::RemoveLast(b.fullPath, &bf);
		return af.compare(bf) < 0;
	});
	ia->Timer = 0;

	if (ia->Compressed.load())
	{
		for (int i = 0; i < ia->FrameCount; i++)
		{
			if (ia->Cancelled.load())
				break;
			File newImage;
			if (newImage.OpenRead(files[i].fullPath)) {
				Buffer newData;
				newData.resize(newImage.GetSize());
				newImage.Read(newData.data(), newImage.GetSize());
				ia->FrameData.push_back(std::move(newData));
			}
		}
		ia->NextImage = ImageRes::Create(ia->FrameData[0]);
	}
	else {
		for (int i = 0; i < ia->FrameCount; i++)
		{
			if (ia->Cancelled.load())
				break;
			ia->Frames.Add(Graphics::ImageRes::Create(files[i].fullPath));
		}
	}
	ia->LoadComplete.store(true);


	if (ia->Compressed.load())
	{
		std::chrono::microseconds sleepDuration((uint32)(250000.f * ia->SecondsPerFrame));
		int currentFrame = -1;
		while (!ia->Cancelled.load())
		{
			if (ia->CurrentFrame != currentFrame)
			{
				currentFrame = ia->CurrentFrame;
				int nextFrame = (currentFrame + 1) % ia->FrameCount;
				Image nextImage = ImageRes::Create(ia->FrameData[nextFrame]);
				ia->LoadMutex.lock();
				ia->NextImage = nextImage;
				ia->LoadMutex.unlock();
			}
			else 
			{
				std::this_thread::sleep_for(sleepDuration);
			}
		}
	}
}

static int lTickAnimation(lua_State* L)
{
	int key;
	float deltatime;
	key = luaL_checkinteger(L, 1);
	deltatime = luaL_checknumber(L, 2);

	if (!g_guiState.animations.Contains(key)) {
		lua_pushinteger(L, -1);
		return 1;
	}

	Ref<ImageAnimation> ia = g_guiState.animations.at(key);
	if (!ia->LoadComplete.load()) {
		lua_pushinteger(L, 0);
		return 1;
	}

	if (ia->Cancelled.load()) {
		lua_pushinteger(L, -1);
		return 1;
	}

	ia->Timer += deltatime;
	if (ia->Timer >= ia->SecondsPerFrame)
	{
		if (ia->LoopCounter < ia->TimesToLoop || ia->TimesToLoop == 0)
		{
			ia->Timer = fmodf(ia->Timer, ia->SecondsPerFrame);

			if (ia->CurrentFrame == ia->FrameCount - 1)
				++ia->LoopCounter;

			if (ia->LoopCounter == ia->TimesToLoop && ia->LoopCounter != 0)
				return 0;

			ia->CurrentFrame = (ia->CurrentFrame + 1) % ia->FrameCount;
			if (ia->Compressed.load())
			{
				ia->LoadMutex.lock();
				ia->CurrentImage = ia->NextImage;
				ia->LoadMutex.unlock();
				nvgUpdateImage(g_guiState.vg, key, (unsigned char*)ia->CurrentImage->GetBits());
			}
			else 
			{
				nvgUpdateImage(g_guiState.vg, key, (unsigned char*)ia->Frames[ia->CurrentFrame]->GetBits());
			}
		}
	}
	lua_pushinteger(L, 1);
	return 1;
}

static int LoadAnimation(lua_State* L, const char* path, float frametime, int loopcount, bool compressed)
{
	Vector<FileInfo> files = Files::ScanFiles(path);
	if (files.empty())
		return -1;

	int key = nvgCreateImage(g_guiState.vg, *files[0].fullPath, 0);
	Ref<ImageAnimation> ia = std::make_shared<ImageAnimation>();
	ia->Compressed = compressed;
	ia->TimesToLoop = loopcount;
	ia->LoopCounter = 0;
	ia->SecondsPerFrame = frametime;
	ia->LoadComplete.store(false);
	ia->Cancelled.store(false);
	ia->State = L;
	ia->JobThread = new Thread(AnimationLoader, files, ia);
	g_guiState.animations.insert(std::make_pair(key, ia));

	return key;
}

static int lResetAnimation(lua_State* L)
{
	int key;
	key = luaL_checkinteger(L, 1);
	Ref<ImageAnimation> ia = g_guiState.animations.at(key);
	ia->CurrentFrame = 0;
	ia->Timer = 0;
	ia->LoopCounter = 0;
	return 0;
}

static int lLoadAnimation(lua_State* L)
{
	const char* path;
	float frametime;
	int loopcount = 0;
	bool compressed = false;

	path = luaL_checkstring(L, 1);
	frametime = luaL_checknumber(L, 2);
	if (lua_gettop(L) == 3)
	{
		loopcount = luaL_checkinteger(L, 3);
	}
	else if (lua_gettop(L) == 4)
	{
		loopcount = luaL_checkinteger(L, 3);
		compressed = lua_toboolean(L, 4) == 1;
	}
	

	int result = LoadAnimation(L, path, frametime, loopcount, compressed);
	if (result == -1)
		return 0;

	lua_pushnumber(L, result);
	return 1;
}

static int lText(lua_State* L /*const char* s, float x, float y*/)
{
	const char* s;
	float x, y;
	s = luaL_checkstring(L, 1);
	x = luaL_checknumber(L, 2);
	y = luaL_checknumber(L, 3);
	nvgText(g_guiState.vg, x, y, s, NULL);

	//{ //Fast text
	//	WString text = Utility::Convert	ToWString(s);
	//	Text te = (*g_guiState.currentFont)->CreateText(text, g_guiState.fontSize);
	//	Transform textTransform = g_guiState.t;
	//	textTransform *= Transform::Translation(Vector2(x, y));

	//	//vertical alignment
	//	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_BOTTOM) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(0, -te->size.y));
	//	}
	//	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_MIDDLE) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(0, -te->size.y / 2));
	//	}

	//	//horizontal alignment
	//	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_CENTER) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(-te->size.x / 2, 0));
	//	}
	//	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_RIGHT) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(-te->size.x, 0));
	//	}
	//	MaterialParameterSet params;
	//	params.SetParameter("color", g_guiState.fillColor);
	//	g_guiState.rq->Draw(textTransform, te, g_application->GetFontMaterial(), params);
	//}
	return 0;
}

static int lFontFace(lua_State* L /*const char* s*/)
{
	const char* s;
	s = luaL_checkstring(L, 1);
	nvgFontFace(g_guiState.vg, s);
	return 0;
}
static int lFontSize(lua_State* L /*float size*/)
{
	float size = luaL_checknumber(L, 1);
	nvgFontSize(g_guiState.vg, size);
	g_guiState.fontSize = size;
	return 0;
}
static int lFillColor(lua_State* L /*int r, int g, int b, int a = 255*/)
{
	int r, g, b, a;
	r = luaL_checkinteger(L, 1);
	g = luaL_checkinteger(L, 2);
	b = luaL_checkinteger(L, 3);
	if (lua_gettop(L) == 4)
	{
		a = luaL_checkinteger(L, 4);
	}
	else
	{
		a = 255;
	}
	nvgFillColor(g_guiState.vg, nvgRGBA(r, g, b, a));
	g_guiState.fillColor = Vector4(r / 255.0, g / 255.0, b / 255.0, a / 255.0);
	return 0;
}
static int lRect(lua_State* L /*float x, float y, float w, float h*/)
{
	float x, y, w, h;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	w = luaL_checknumber(L, 3);
	h = luaL_checknumber(L, 4);
	nvgRect(g_guiState.vg, x, y, w, h);
	return 0;
}
static int lFill(lua_State* L)
{
	nvgFill(g_guiState.vg);
	return 0;
}
static int lTextAlign(lua_State* L /*int align*/)
{
	nvgTextAlign(g_guiState.vg, luaL_checkinteger(L, 1));
	g_guiState.textAlign = luaL_checkinteger(L, 1);
	return 0;
}
static int lCreateImage(lua_State* L /*const char* filename, int imageflags */)
{
	const char* filename = luaL_checkstring(L, 1);
	int imageflags = luaL_checkinteger(L, 2);
	int handle = nvgCreateImage(g_guiState.vg, filename, imageflags);
	if (handle != 0)
	{
		g_guiState.vgImages[L].Add(handle);
		lua_pushnumber(L, handle);
		return 1;
	}
	return 0;
}
static int lImagePatternFill(lua_State* L /*int image, float alpha*/)
{
	int image = luaL_checkinteger(L, 1);
	float alpha = luaL_checknumber(L, 2);
	int w, h;
	nvgImageSize(g_guiState.vg, image, &w, &h);
	nvgFillPaint(g_guiState.vg, nvgImagePattern(g_guiState.vg, 0, 0, w, h, 0, image, alpha));
	return 0;
}

static int lSetImageTint(lua_State* L /*int r, int g, int b*/)
{
	int r, g, b;
	r = luaL_checkinteger(L, 1);
	g = luaL_checkinteger(L, 2);
	b = luaL_checkinteger(L, 3);
	g_guiState.imageTint = nvgRGB(r, g, b);
	return 0;
}

static int lImageRect(lua_State* L /*float x, float y, float w, float h, int image, float alpha, float angle*/)
{
	float x = 0.f;
	float y = 0.f;
	float w = 0.f;
	float h = 0.f;
	float alpha = 1.f;
	float angle = 0.f;
	int image = -1;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	w = luaL_checknumber(L, 3);
	h = luaL_checknumber(L, 4);
	image = luaL_checkinteger(L, 5);
	alpha = luaL_checknumber(L, 6);
	angle = luaL_checknumber(L, 7);

	int imgH = -1, imgW = -1;
	nvgImageSize(g_guiState.vg, image, &imgW, &imgH);
	float scaleX = 1.f, scaleY = 1.f;
	float tr[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
	nvgCurrentTransform(g_guiState.vg, tr);
	scaleX = w / imgW;
	scaleY = h / imgH;
	nvgTranslate(g_guiState.vg, x, y);
	nvgRotate(g_guiState.vg, angle);
	nvgScale(g_guiState.vg, scaleX, scaleY);
	NVGpaint paint = nvgImagePattern(g_guiState.vg, 0, 0, imgW, imgH, 0, image, alpha);
	paint.innerColor = g_guiState.imageTint;
	paint.innerColor.a = alpha;
	nvgFillPaint(g_guiState.vg, paint);
	nvgRect(g_guiState.vg, 0, 0, imgW, imgH);
	nvgFill(g_guiState.vg);
	nvgResetTransform(g_guiState.vg);
	nvgTransform(g_guiState.vg, tr[0], tr[1], tr[2], tr[3], tr[4], tr[5]);
	return 0;
}
static int lScale(lua_State* L /*float x, float y*/)
{
	float x, y;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	nvgScale(g_guiState.vg, x, y);
	g_guiState.t *= Transform::Scale({ x, y, 0 });
	return 0;
}
static int lTranslate(lua_State* L /*float x, float y*/)
{
	float x, y;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	g_guiState.t *= Transform::Translation({ x, y, 0 });
	nvgTranslate(g_guiState.vg, x, y);
	return 0;
}
static int lRotate(lua_State* L /*float angle*/)
{
	float angle = luaL_checknumber(L, 1);
	nvgRotate(g_guiState.vg, angle);
	g_guiState.t *= Transform::Rotation({ 0, 0, angle });
	return 0;
}
static int lResetTransform(lua_State* L)
{
	nvgResetTransform(g_guiState.vg);
	g_guiState.t = Transform();
	return 0;
}
static int lLoadFont(lua_State* L /*const char* name, const char* filename*/)
{
	const char* name = luaL_checkstring(L, 1);
	const char* filename = luaL_checkstring(L, 2);
	return LoadFont(name, filename, L);
}
static int lCreateLabel(lua_State* L /*const char* text, int size, bool monospace*/)
{
	const char* text = luaL_checkstring(L, 1);
	int size = luaL_checkinteger(L, 2);
	int monospace = luaL_checkinteger(L, 3);

	Label newLabel;
	newLabel.text = g_guiState.currentFont->CreateText(Utility::ConvertToWString(text), 
		size * g_guiState.t.GetScale().y,
		(FontRes::TextOptions)monospace);
	newLabel.scale = g_guiState.t.GetScale().y;
	newLabel.size = size;
	newLabel.opt = (FontRes::TextOptions)monospace;
	newLabel.font = g_guiState.currentFont;
	newLabel.content = text;
	g_guiState.textCache.FindOrAdd(L).Add(g_guiState.nextTextId[L], newLabel);
	lua_pushnumber(L, g_guiState.nextTextId[L]);
	g_guiState.nextTextId[L]++;
	return 1;
}

static int lUpdateLabel(lua_State* L /*int labelId, const char* text, int size*/)
{
	int labelId = luaL_checkinteger(L, 1);
	const char* text = luaL_checkstring(L, 2);
	int size = luaL_checkinteger(L, 3);
	Label updated;
	updated.text = g_guiState.currentFont->CreateText(Utility::ConvertToWString(text), size * g_guiState.t.GetScale().y);
	updated.size = size;
	updated.scale = g_guiState.t.GetScale().y;
	updated.content = text;
	g_guiState.textCache[L][labelId] = updated;
	return 0;
}

static int lDrawLabel(lua_State* L /*int labelId, float x, float y, float maxWidth = -1*/)
{
	int labelId = luaL_checkinteger(L, 1);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float maxWidth = -1;
	if (lua_gettop(L) == 4)
	{
		maxWidth = luaL_checknumber(L, 4);
	}
	Vector2 scale = g_guiState.t.GetScale().xy();
	if (scale.x == 0 || scale.y == 0)
		return 0;

	Transform textTransform = g_guiState.t;
	textTransform *= Transform::Translation(Vector2(x, y));
	textTransform *= Transform::Scale(Vector2(1.0) / scale);
	Label te = g_guiState.textCache[L][labelId];
	if (fabsf(te.scale - g_guiState.t.GetScale().y) > 0.001)
	{
		te.scale = g_guiState.t.GetScale().y;
		te.text = te.font->CreateText(Utility::ConvertToWString(te.content), Math::Round((float)te.size * te.scale));
		g_guiState.textCache[L][labelId] = te;
	}
	float mwScale = 1.0f;
	if (maxWidth > 0)
	{
		mwScale = Math::Min(1.0f, maxWidth / ((float)te.text->size.x / te.scale));
		textTransform *= Transform::Scale(Vector2(mwScale));
	}

	//vertical alignment
	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_BOTTOM) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te.text->size.y));
	}
	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_MIDDLE) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te.text->size.y / 2));
	}

	//horizontal alignment
	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_CENTER) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te.text->size.x / 2, 0));
	}
	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_RIGHT) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te.text->size.x, 0));
	}


	MaterialParameterSet params;
	params.SetParameter("color", g_guiState.fillColor);
	g_guiState.rq->DrawScissored(g_guiState.scissor ,textTransform, te.text, *g_guiState.fontMaterial, params);
	return 0;
}

static int lMoveTo(lua_State* L /* float x, float y */)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	nvgMoveTo(g_guiState.vg, x, y);
	return 0;
}

static int lLineTo(lua_State* L /* float x, float y */)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	nvgLineTo(g_guiState.vg, x, y);
	return 0;
}

static int lBezierTo(lua_State* L /* float c1x, float c1y, float c2x, float c2y, float x, float y */)
{
	float c1x = luaL_checknumber(L, 1);
	float c1y = luaL_checknumber(L, 2);
	float c2x = luaL_checknumber(L, 3);
	float c2y = luaL_checknumber(L, 4);
	float x = luaL_checknumber(L, 5);
	float y = luaL_checknumber(L, 6);
	nvgBezierTo(g_guiState.vg, c1x, c1y, c2x, c2y, x, y);
	return 0;
}

static int lQuadTo(lua_State* L /* float cx, float cy, float x, float y */)
{
	float cx = luaL_checknumber(L, 1);
	float cy = luaL_checknumber(L, 2);
	float x = luaL_checknumber(L, 3);
	float y = luaL_checknumber(L, 4);
	nvgQuadTo(g_guiState.vg, cx, cy, x, y);
	return 0;
}

static int lArcTo(lua_State* L /* float x1, float y1, float x2, float y2, float radius */)
{
	float x1 = luaL_checknumber(L, 1);
	float y1 = luaL_checknumber(L, 2);
	float x2 = luaL_checknumber(L, 3);
	float y2 = luaL_checknumber(L, 4);
	float radius = luaL_checknumber(L, 5);
	nvgArcTo(g_guiState.vg, x1, y1, x2, y2, radius);
	return 0;
}

static int lClosePath(lua_State* L)
{
	nvgClosePath(g_guiState.vg);
	return 0;
}

static int lStroke(lua_State* L)
{
	nvgStroke(g_guiState.vg);
	return 0;
}

static int lMiterLimit(lua_State* L /* float limit */)
{
	float limit = luaL_checknumber(L, 1);
	nvgMiterLimit(g_guiState.vg, limit);
	return 0;
}

static int lStrokeWidth(lua_State* L /* float size */)
{
	float size = luaL_checknumber(L, 1);
	nvgStrokeWidth(g_guiState.vg, size);
	return 0;
}

static int lLineCap(lua_State* L /* int cap */)
{
	int cap = luaL_checkinteger(L, 1);
	nvgLineCap(g_guiState.vg, cap);
	return 0;
}

static int lLineJoin(lua_State* L /* int join */)
{
	int join = luaL_checkinteger(L, 1);
	nvgLineJoin(g_guiState.vg, join);
	return 0;
}

static int lStrokeColor(lua_State* L /*int r, int g, int b, int a = 255*/)
{
	int r, g, b, a;
	r = luaL_checkinteger(L, 1);
	g = luaL_checkinteger(L, 2);
	b = luaL_checkinteger(L, 3);
	if (lua_gettop(L) == 4)
	{
		a = luaL_checkinteger(L, 4);
	}
	else
	{
		a = 255;
	}
	nvgStrokeColor(g_guiState.vg, nvgRGBA(r, g, b, a));
	return 0;
}

static int lFastRect(lua_State* L /*float x, float y, float w, float h*/)
{
	float x, y, w, h;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	w = luaL_checknumber(L, 3);
	h = luaL_checknumber(L, 4);
	Mesh quad = Graphics::MeshGenerators::Quad(g_gl, Vector2(x, y), Vector2(w, h));
	MaterialParameterSet params;
	params.SetParameter("color", g_guiState.fillColor);
	g_guiState.rq->DrawScissored(g_guiState.scissor, g_guiState.t, quad, *g_guiState.fillMaterial, params);
	return 0;
}

static int lFastText(lua_State* L /* String utf8string, float x, float y */)
{
	const char* s;
	float x, y;
	s = luaL_checkstring(L, 1);
	x = luaL_checknumber(L, 2);
	y = luaL_checknumber(L, 3);

	WString text = Utility::ConvertToWString(s);
	Text te = g_guiState.currentFont->CreateText(text, g_guiState.fontSize);
	Transform textTransform = g_guiState.t;
	textTransform *= Transform::Translation(Vector2(x, y));

	//vertical alignment
	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_BOTTOM) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te->size.y));
	}
	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_MIDDLE) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te->size.y / 2));
	}

	//horizontal alignment
	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_CENTER) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te->size.x / 2, 0));
	}
	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_RIGHT) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te->size.x, 0));
	}
	MaterialParameterSet params;
	params.SetParameter("color", g_guiState.fillColor);
	g_guiState.rq->DrawScissored(g_guiState.scissor, textTransform, te, *g_guiState.fontMaterial, params);

	return 0;
}

static int lRoundedRect(lua_State* L /* float x, float y, float w, float h, float r */)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);
	float r = luaL_checknumber(L, 5);
	nvgRoundedRect(g_guiState.vg, x, y, w, h, r);
	return 0;
}

static int lRoundedRectVarying(lua_State* L /* float x, float y, float w, float h, float radTopLeft, float radTopRight, float radBottomRight, float radBottomLeft */)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);
	float radTopLeft = luaL_checknumber(L, 5);
	float radTopRight = luaL_checknumber(L, 6);
	float radBottomRight = luaL_checknumber(L, 7);
	float radBottomLeft = luaL_checknumber(L, 8);
	nvgRoundedRectVarying(g_guiState.vg, x, y, w, h, radTopLeft, radTopRight, radBottomRight, radBottomLeft);
	return 0;
}

static int lEllipse(lua_State* L /* float cx, float cy, float rx, float ry */)
{
	float cx = luaL_checknumber(L, 1);
	float cy = luaL_checknumber(L, 2);
	float rx = luaL_checknumber(L, 3);
	float ry = luaL_checknumber(L, 4);
	nvgEllipse(g_guiState.vg, cx, cy, rx, ry);
	return 0;
}

static int lCircle(lua_State* L /* float cx, float cy, float r */)
{
	float cx = luaL_checknumber(L, 1);
	float cy = luaL_checknumber(L, 2);
	float r = luaL_checknumber(L, 3);
	nvgCircle(g_guiState.vg, cx, cy, r);
	return 0;
}

static int lSkewX(lua_State* L /* float angle */)
{
	float angle = luaL_checknumber(L, 1);
	nvgSkewX(g_guiState.vg, angle);
	return 0;
}

static int lSkewY(lua_State* L /* float angle */)
{
	float angle = luaL_checknumber(L, 1);
	nvgSkewY(g_guiState.vg, angle);
	return 0;
}

static int lLinearGradient(lua_State* L /* float sx, float sy, float ex, float ey */)
{
	float sx = luaL_checknumber(L, 1);
	float sy = luaL_checknumber(L, 2);
	float ex = luaL_checknumber(L, 3);
	float ey = luaL_checknumber(L, 4);
	NVGpaint paint = nvgLinearGradient(g_guiState.vg, sx, sy, ex, ey, g_guiState.inrColor, g_guiState.otrColor);
	g_guiState.paintCache[L].Add(g_guiState.nextPaintId[L], paint);
	lua_pushnumber(L, g_guiState.nextPaintId[L]);
	g_guiState.nextPaintId[L]++;
	return 1;
}

static int lBoxGradient(lua_State* L /* float x, float y, float w, float h, float r, float f */)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);
	float r = luaL_checknumber(L, 5);
	float f = luaL_checknumber(L, 6);
	NVGpaint paint = nvgBoxGradient(g_guiState.vg, x, y, w, h, r, f, g_guiState.inrColor, g_guiState.otrColor);
	g_guiState.paintCache[L].Add(g_guiState.nextPaintId[L], paint);
	lua_pushnumber(L, g_guiState.nextPaintId[L]);
	g_guiState.nextPaintId[L]++;
	return 1;
}

static int lRadialGradient(lua_State* L /* float cx, float cy, float inr, float outr */)
{
	float cx = luaL_checknumber(L, 1);
	float cy = luaL_checknumber(L, 2);
	float inr = luaL_checknumber(L, 3);
	float outr = luaL_checknumber(L, 4);
	NVGpaint paint = nvgRadialGradient(g_guiState.vg, cx, cy, inr, outr, g_guiState.inrColor, g_guiState.otrColor);
	g_guiState.paintCache[L].Add(g_guiState.nextPaintId[L], paint);
	lua_pushnumber(L, g_guiState.nextPaintId[L]);
	g_guiState.nextPaintId[L]++;
	return 1;
}

static int lImagePattern(lua_State* L /* float ox, float oy, float ex, float ey, float angle, int image, float alpha */)
{
	float ox = luaL_checknumber(L, 1);
	float oy = luaL_checknumber(L, 2);
	float ex = luaL_checknumber(L, 3);
	float ey = luaL_checknumber(L, 4);
	float angle = luaL_checknumber(L, 5);
	int image = luaL_checkinteger(L, 6);
	float alpha = luaL_checknumber(L, 7);
	NVGpaint paint = nvgImagePattern(g_guiState.vg, ox, oy, ex, ey, angle, image, alpha);
	g_guiState.paintCache[L].Add(g_guiState.nextPaintId[L], paint);
	lua_pushnumber(L, g_guiState.nextPaintId[L]);
	g_guiState.nextPaintId[L]++;
	return 1;
}

static int lUpdateImagePattern(lua_State* L /*int paint, float ox, float oy, float ex, float ey, float angle, float alpha*/)
{
	int paint = luaL_checkinteger(L, 1);
	if (!g_guiState.paintCache[L].Contains(paint))
		return 0;
	NVGpaint& p = g_guiState.paintCache[L].at(paint);

	float ox = luaL_checknumber(L, 2);
	float oy = luaL_checknumber(L, 3);
	float ex = luaL_checknumber(L, 4);
	float ey = luaL_checknumber(L, 5);
	float angle = luaL_checknumber(L, 6);
	float alpha = luaL_checknumber(L, 7);

	nvgTransformIdentity(p.xform);
	nvgTransformRotate(p.xform, angle);
	p.xform[4] = ox;
	p.xform[5] = oy;

	p.extent[0] = ex;
	p.extent[1] = ey;

	p.innerColor.a = p.outerColor.a = alpha;
	return 0;
}

static int lGradientColors(lua_State* L /*int ri, int gi, int bi, int ai, int ro, int go, int bo, int ao*/)
{
	int ri, gi, bi, ai, ro, go, bo, ao;
	ri = luaL_checkinteger(L, 1);
	gi = luaL_checkinteger(L, 2);
	bi = luaL_checkinteger(L, 3);
	ai = luaL_checkinteger(L, 4);
	ro = luaL_checkinteger(L, 5);
	go = luaL_checkinteger(L, 6);
	bo = luaL_checkinteger(L, 7);
	ao = luaL_checkinteger(L, 8);
	g_guiState.inrColor = nvgRGBA(ri, gi, bi, ai);
	g_guiState.otrColor = nvgRGBA(ro, go, bo, ao);
	return 0;
}

static int lFillPaint(lua_State* L /* int paint */)
{
	int paint = luaL_checkinteger(L, 1);
	nvgFillPaint(g_guiState.vg, g_guiState.paintCache[L][paint]);
	return 0;
}

static int lStrokePaint(lua_State* L /* int paint */)
{
	int paint = luaL_checkinteger(L, 1);
	nvgStrokePaint(g_guiState.vg, g_guiState.paintCache[L][paint]);
	return 0;
}

static int lSave(lua_State* L /*  */)
{
	g_guiState.transformStack.push_back(g_guiState.t);
	nvgSave(g_guiState.vg);
	return 0;
}

static int lRestore(lua_State* L /*  */)
{
	g_guiState.t = g_guiState.transformStack.back();
	g_guiState.transformStack.pop_back();
	nvgRestore(g_guiState.vg);
	return 0;
}

static int lReset(lua_State* L /*  */)
{
	g_guiState.transformStack.clear();
	nvgReset(g_guiState.vg);
	return 0;
}

static int lPathWinding(lua_State* L /* int dir */)
{
	int dir = luaL_checkinteger(L, 1);
	nvgPathWinding(g_guiState.vg, dir);
	return 0;
}

static int lScissor(lua_State* L /* float x, float y, float w, float h */)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);
	
	Vector3 scale = g_guiState.t.GetScale();
	Vector3 pos = g_guiState.t.GetPosition();
	Vector2 topLeft = pos.xy() + Vector2(x + g_guiState.scissorOffset, y);
	Vector2 size = Vector2(w, h) * scale.xy();


	g_guiState.scissor = Rect(topLeft, size);
	
	nvgScissor(g_guiState.vg, x, y, w, h);
	return 0;
}

static int lIntersectScissor(lua_State* L /* float x, float y, float w, float h */)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);
	nvgIntersectScissor(g_guiState.vg, x, y, w, h);
	return 0;
}

static int lResetScissor(lua_State* L /*  */)
{
	nvgResetScissor(g_guiState.vg);
	g_guiState.scissor = Rect(0, 0, -1, -1);
	return 0;
}

static int lTextBounds(lua_State* L /*float x, float y, char* s*/)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	const char* s = luaL_checkstring(L, 3);
	float bounds[4] = { 0,0,0,0 };

	nvgTextBounds(g_guiState.vg, x, y, s, 0, bounds);
	for (size_t i = 0; i < 4; i++)
	{
		lua_pushnumber(L, bounds[i]);
	}
	return 4;
}

static int lLabelSize(lua_State* L /*int label*/)
{
	int label = luaL_checkinteger(L, 1);
	Label l = g_guiState.textCache[L][label];
	lua_pushnumber(L, l.text->size.x / l.scale);
	lua_pushnumber(L, l.text->size.y / l.scale);
	return 2;
}

static int lFastTextSize(lua_State* L /* char* text */)
{
	const char* s;
	s = luaL_checkstring(L, 1);

	WString text = Utility::ConvertToWString(s);
	Text l = g_guiState.currentFont->CreateText(text, g_guiState.fontSize);
	lua_pushnumber(L, l->size.x);
	lua_pushnumber(L, l->size.y);
	return 2;
}
static int lImageSize(lua_State* L /*int image*/)
{
		int image = luaL_checkinteger(L, 1);
		int w,h;
		nvgImageSize(g_guiState.vg, image, &w, &h);
		lua_pushnumber(L,w);
		lua_pushnumber(L,h);
		return 2;
}
static int DisposeGUI(lua_State* state)
{
	g_guiState.textCache[state].clear();
	g_guiState.textCache.erase(state);
	g_guiState.paintCache[state].clear();
	g_guiState.paintCache.erase(state);

	
	for(auto&& i : g_guiState.vgImages[state])
	{
		nvgDeleteImage(g_guiState.vg, i);
	}


	Vector<int> keysToDelete;
	for (auto anim : g_guiState.animations)
	{
		if (anim.second->State != state)
			continue;

		anim.second->Cancelled.store(true);

		if(anim.second->JobThread && anim.second->JobThread->joinable())
			anim.second->JobThread->join();
		anim.second->Frames.clear();
		anim.second->FrameData.clear();
		keysToDelete.Add(anim.first);
		delete anim.second->JobThread;
		nvgDeleteImage(g_guiState.vg, anim.first);
	}
	for (int k : keysToDelete)
	{
		g_guiState.animations.erase(k);
	}

	return 0;
}

static int lArc(lua_State* L /* float cx, float cy, float r, float a0, float a1, int dir */)
{
	float cx = luaL_checknumber(L, 1);
	float cy = luaL_checknumber(L, 2);
	float r = luaL_checknumber(L, 3);
	float a0 = luaL_checknumber(L, 4);
	float a1 = luaL_checknumber(L, 5);
	int dir = luaL_checkinteger(L, 6);
	nvgArc(g_guiState.vg, cx, cy, r, a0, a1, dir);
	return 0;
}

static int lGlobalCompositeOperation(lua_State* L /* int op */)
{
	int op = luaL_checkinteger(L, 1);
	nvgGlobalCompositeOperation(g_guiState.vg, op);
	return 0;
}

static int lGlobalCompositeBlendFunc(lua_State* L /* int sfactor, int dfactor */)
{
	int sfactor = luaL_checkinteger(L, 1);
	int dfactor = luaL_checkinteger(L, 2);
	nvgGlobalCompositeBlendFunc(g_guiState.vg, sfactor, dfactor);
	return 0;
}

static int lGlobalCompositeBlendFuncSeparate(lua_State* L /* int srcRGB, int dstRGB, int srcAlpha, int dstAlpha */)
{
	int srcRGB = luaL_checkinteger(L, 1);
	int dstRGB = luaL_checkinteger(L, 2);
	int srcAlpha = luaL_checkinteger(L, 3);
	int dstAlpha = luaL_checkinteger(L, 4);
	nvgGlobalCompositeBlendFuncSeparate(g_guiState.vg, srcRGB, dstRGB, srcAlpha, dstAlpha);
	return 0;
}

static int lGlobalAlpha(lua_State* L /*float alpha*/)
{
	nvgGlobalAlpha(g_guiState.vg, luaL_checknumber(L, 1));
	return 0;
}