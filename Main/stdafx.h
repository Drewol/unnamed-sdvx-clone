/* Main and precompiled header file for Main project*/
#pragma once

// OpenGL headers
#include <Graphics/GL.hpp>

#ifdef _WIN32
// Windows Header Files:
#include <windows.h>
#include <tchar.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>

// C RunTime Header Files
#include <stdlib.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include <memory.h>
#include <time.h>

#include <cinttypes>
#include <ctime>

#include <functional>
#include <memory>
#include <random>
#include <stack>
#include <string>
#include <queue>
#include <unordered_set>

#include <Shared/Shared.hpp>
#include <Shared/Files.hpp>
#include <Shared/Jobs.hpp>
#include <Shared/Thread.hpp>

#include <Audio/Sample.hpp>

// Graphics components
#include <Graphics/OpenGL.hpp>
#include <Graphics/Image.hpp>
#include <Graphics/ImageLoader.hpp>
#include <Graphics/Texture.hpp>
#include <Graphics/Material.hpp>
#include <Graphics/Mesh.hpp>
#include <Graphics/RenderQueue.hpp>
#include <Graphics/RenderState.hpp>
#include <Graphics/ParticleSystem.hpp>
#include <Graphics/MeshGenerators.hpp>
#include <Graphics/Font.hpp>
using namespace Graphics;

#include "archive.h"
#include "archive_entry.h"
#include "cpr/cpr.h"
#include "discord_rpc.h"
#include "json.hpp"
#include "lua.hpp"
#include "nanovg.h"

// NK imports
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#undef NK_IMPLEMENTATION
#ifdef EMBEDDED
#undef NK_SDL_GLES2_IMPLEMENTATION
#include "../third_party/nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gles2.h"
#else
#undef	NK_SDL_GL3_IMPLEMENTATION
#include "../third_party/nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"
#endif

constexpr int MAX_VERTEX_MEMORY = 512 * 1024;
constexpr int MAX_ELEMENT_MEMORY = 128 * 1024;
constexpr int FULL_FONT_TEXTURE_HEIGHT = 8192; //needed to load all CJK glyphs

#include "BasicDefinitions.hpp"

// Commonly-used headers which are unlikely to change
#include "ApplicationTickable.hpp"
#include "LuaRequests.hpp"

// Asset loading macro
#define CheckedLoad(__stmt) if(!(__stmt)){Logf("Failed to load asset [%s]", Logger::Severity::Error, #__stmt); return false; }

/// nk_sdl_font_stash_end but uses a better bake function to reduce baked font texture dimension
void usc_nk_sdl_font_stash_end();
const void* usc_nk_bake_atlas(nk_font_atlas * atlas, int& w, int& h);
GLuint usc_nk_sdl_generate_texture(nk_font_atlas * atlas, const void* image, int w, int h);
void usc_nk_sdl_use_atlas(nk_font_atlas * atlas, GLuint texture);
void nk_sdl_device_destroy_keep_font(void);
void nk_sdl_shutdown_keep_font(void);
void nk_atlas_font_stash_begin(struct nk_font_atlas* atlas);
