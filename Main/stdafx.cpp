/* Used to generate precompiled header file for Main project*/
#include "stdafx.h"

// nuklear's nk_dtoa is inaccurate, even for exact values like 0.25f.
static inline void sprintf_dtoa(char(&buffer)[64 /* NK_MAX_NUMBER_BUFFER */], double d)
{
	Utility::BufferSprintf(buffer, "%g", d);
}

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION

#define NK_DTOA sprintf_dtoa

#ifdef EMBEDDED
#define NK_SDL_GLES2_IMPLEMENTATION
#include "../third_party/nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gles2.h"
#else
#define NK_SDL_GL3_IMPLEMENTATION
#include "../third_party/nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"
#endif

/// nk_font_bake_pack but with better logic for width determination
/// from https://github.com/vurtun/nuklear/issues/738
static inline int usc_nk_font_bake_pack(struct nk_font_baker* baker,
    nk_size* image_memory, int* width, int* height, struct nk_recti* custom,
    const struct nk_font_config* config_list, int count,
    struct nk_allocator* alloc)
{
    NK_STORAGE const nk_size max_height = 1024 * 32;
    const struct nk_font_config* config_iter, * it;
    int total_glyph_count = 0;
    int total_range_count = 0;
    int range_count = 0;
    int i = 0;

    NK_ASSERT(image_memory);
    NK_ASSERT(width);
    NK_ASSERT(height);
    NK_ASSERT(config_list);
    NK_ASSERT(count);
    NK_ASSERT(alloc);

    int max_size = 0;

    if (!image_memory || !width || !height || !config_list || !count) return nk_false;
    for (config_iter = config_list; config_iter; config_iter = config_iter->next) {
        it = config_iter;
        do {
            range_count = nk_range_count(it->range);
            total_range_count += range_count;
            total_glyph_count += nk_range_glyph_count(it->range, range_count);
            if (it->size > max_size) max_size = it->size;
        } while ((it = it->n) != config_iter);
    }
    /* setup font baker from temporary memory */
    for (config_iter = config_list; config_iter; config_iter = config_iter->next) {
        it = config_iter;
        do {
            if (!nk_tt_InitFont(&baker->build[i++].info, (const unsigned char*)it->ttf_blob, 0))
                return nk_false;
        } while ((it = it->n) != config_iter);
    }
    *height = 0;
    *width = nk_round_up_pow2(static_cast<int>(max_size * sqrt(total_glyph_count)));
    nk_tt_PackBegin(&baker->spc, 0, (int)*width, (int)max_height, 0, 1, alloc);
    {
        int input_i = 0;
        int range_n = 0;
        int rect_n = 0;
        int char_n = 0;

        if (custom) {
            /* pack custom user data first so it will be in the upper left corner*/
            struct nk_rp_rect custom_space;
            nk_zero(&custom_space, sizeof(custom_space));
            custom_space.w = (nk_rp_coord)(custom->w);
            custom_space.h = (nk_rp_coord)(custom->h);

            nk_tt_PackSetOversampling(&baker->spc, 1, 1);
            nk_rp_pack_rects((struct nk_rp_context*)baker->spc.pack_info, &custom_space, 1);
            *height = NK_MAX(*height, (int)(custom_space.y + custom_space.h));

            custom->x = (short)custom_space.x;
            custom->y = (short)custom_space.y;
            custom->w = (short)custom_space.w;
            custom->h = (short)custom_space.h;
        }

        /* first font pass: pack all glyphs */
        for (input_i = 0, config_iter = config_list; input_i < count && config_iter;
            config_iter = config_iter->next) {
            it = config_iter;
            do {
                int n = 0;
                int glyph_count;
                const nk_rune* in_range;
                const struct nk_font_config* cfg = it;
                struct nk_font_bake_data* tmp = &baker->build[input_i++];

                /* count glyphs + ranges in current font */
                glyph_count = 0; range_count = 0;
                for (in_range = cfg->range; in_range[0] && in_range[1]; in_range += 2) {
                    glyph_count += (int)(in_range[1] - in_range[0]) + 1;
                    range_count++;
                }

                /* setup ranges  */
                tmp->ranges = baker->ranges + range_n;
                tmp->range_count = (nk_rune)range_count;
                range_n += range_count;
                for (i = 0; i < range_count; ++i) {
                    in_range = &cfg->range[i * 2];
                    tmp->ranges[i].font_size = cfg->size;
                    tmp->ranges[i].first_unicode_codepoint_in_range = (int)in_range[0];
                    tmp->ranges[i].num_chars = (int)(in_range[1] - in_range[0]) + 1;
                    tmp->ranges[i].chardata_for_range = baker->packed_chars + char_n;
                    char_n += tmp->ranges[i].num_chars;
                }

                /* pack */
                tmp->rects = baker->rects + rect_n;
                rect_n += glyph_count;
                nk_tt_PackSetOversampling(&baker->spc, cfg->oversample_h, cfg->oversample_v);
                n = nk_tt_PackFontRangesGatherRects(&baker->spc, &tmp->info,
                    tmp->ranges, (int)tmp->range_count, tmp->rects);
                nk_rp_pack_rects((struct nk_rp_context*)baker->spc.pack_info, tmp->rects, (int)n);

                /* texture height */
                for (i = 0; i < n; ++i) {
                    if (tmp->rects[i].was_packed)
                        *height = NK_MAX(*height, tmp->rects[i].y + tmp->rects[i].h);
                }
            } while ((it = it->n) != config_iter);
        }
        NK_ASSERT(rect_n == total_glyph_count);
        NK_ASSERT(char_n == total_glyph_count);
        NK_ASSERT(range_n == total_range_count);
    }
    *height = (int)nk_round_up_pow2((nk_uint)*height);
    *image_memory = (nk_size)(*width) * (nk_size)(*height);
    return nk_true;
}

/// nk_font_atlas_bake but calls usc_nk_font_bake_pack
static inline const void* usc_nk_font_atlas_bake(struct nk_font_atlas* atlas, int* width, int* height,
    enum nk_font_atlas_format fmt)
{
    int i = 0;
    void* tmp = 0;
    nk_size tmp_size, img_size;
    struct nk_font* font_iter;
    struct nk_font_baker* baker;

    NK_ASSERT(atlas);
    NK_ASSERT(atlas->temporary.alloc);
    NK_ASSERT(atlas->temporary.free);
    NK_ASSERT(atlas->permanent.alloc);
    NK_ASSERT(atlas->permanent.free);

    NK_ASSERT(width);
    NK_ASSERT(height);
    if (!atlas || !width || !height ||
        !atlas->temporary.alloc || !atlas->temporary.free ||
        !atlas->permanent.alloc || !atlas->permanent.free)
        return 0;

#ifdef NK_INCLUDE_DEFAULT_FONT
    /* no font added so just use default font */
    if (!atlas->font_num)
        atlas->default_font = nk_font_atlas_add_default(atlas, 13.0f, 0);
#endif
    NK_ASSERT(atlas->font_num);
    if (!atlas->font_num) return 0;

    /* allocate temporary baker memory required for the baking process */
    nk_font_baker_memory(&tmp_size, &atlas->glyph_count, atlas->config, atlas->font_num);
    tmp = atlas->temporary.alloc(atlas->temporary.userdata, 0, tmp_size);
    NK_ASSERT(tmp);
    if (!tmp) goto failed;

    /* allocate glyph memory for all fonts */
    baker = nk_font_baker(tmp, atlas->glyph_count, atlas->font_num, &atlas->temporary);
    atlas->glyphs = (struct nk_font_glyph*)atlas->permanent.alloc(
        atlas->permanent.userdata, 0, sizeof(struct nk_font_glyph) * (nk_size)atlas->glyph_count);
    NK_ASSERT(atlas->glyphs);
    if (!atlas->glyphs)
        goto failed;

    /* pack all glyphs into a tight fit space */
    atlas->custom.w = (NK_CURSOR_DATA_W * 2) + 1;
    atlas->custom.h = NK_CURSOR_DATA_H + 1;
    if (!usc_nk_font_bake_pack(baker, &img_size, width, height, &atlas->custom,
        atlas->config, atlas->font_num, &atlas->temporary))
        goto failed;

    /* allocate memory for the baked image font atlas */
    atlas->pixel = atlas->temporary.alloc(atlas->temporary.userdata, 0, img_size);
    NK_ASSERT(atlas->pixel);
    if (!atlas->pixel)
        goto failed;

    /* bake glyphs and custom white pixel into image */
    nk_font_bake(baker, atlas->pixel, *width, *height,
        atlas->glyphs, atlas->glyph_count, atlas->config, atlas->font_num);
    nk_font_bake_custom_data(atlas->pixel, *width, *height, atlas->custom,
        nk_custom_cursor_data, NK_CURSOR_DATA_W, NK_CURSOR_DATA_H, '.', 'X');

    if (fmt == NK_FONT_ATLAS_RGBA32) {
        /* convert alpha8 image into rgba32 image */
        void* img_rgba = atlas->temporary.alloc(atlas->temporary.userdata, 0,
            (nk_size)(*width * *height * 4));
        NK_ASSERT(img_rgba);
        if (!img_rgba) goto failed;
        nk_font_bake_convert(img_rgba, *width, *height, atlas->pixel);
        atlas->temporary.free(atlas->temporary.userdata, atlas->pixel);
        atlas->pixel = img_rgba;
    }
    atlas->tex_width = *width;
    atlas->tex_height = *height;

    /* initialize each font */
    for (font_iter = atlas->fonts; font_iter; font_iter = font_iter->next) {
        struct nk_font* font = font_iter;
        struct nk_font_config* config = font->config;
        nk_font_init(font, config->size, config->fallback_glyph, atlas->glyphs,
            config->font, nk_handle_ptr(0));
    }

    /* initialize each cursor */
    {NK_STORAGE const struct nk_vec2 nk_cursor_data[NK_CURSOR_COUNT][3] = {
        /* Pos      Size        Offset */
        {{ 0, 3},   {12,19},    { 0, 0}},
        {{13, 0},   { 7,16},    { 4, 8}},
        {{31, 0},   {23,23},    {11,11}},
        {{21, 0},   { 9, 23},   { 5,11}},
        {{55,18},   {23, 9},    {11, 5}},
        {{73, 0},   {17,17},    { 9, 9}},
        {{55, 0},   {17,17},    { 9, 9}}
    };
    for (i = 0; i < NK_CURSOR_COUNT; ++i) {
        struct nk_cursor* cursor = &atlas->cursors[i];
        cursor->img.w = (unsigned short)*width;
        cursor->img.h = (unsigned short)*height;
        cursor->img.region[0] = (unsigned short)(atlas->custom.x + nk_cursor_data[i][0].x);
        cursor->img.region[1] = (unsigned short)(atlas->custom.y + nk_cursor_data[i][0].y);
        cursor->img.region[2] = (unsigned short)nk_cursor_data[i][1].x;
        cursor->img.region[3] = (unsigned short)nk_cursor_data[i][1].y;
        cursor->size = nk_cursor_data[i][1];
        cursor->offset = nk_cursor_data[i][2];
    }}
    /* free temporary memory */
    atlas->temporary.free(atlas->temporary.userdata, tmp);
    return atlas->pixel;

failed:
    /* error so cleanup all memory */
    if (tmp) atlas->temporary.free(atlas->temporary.userdata, tmp);
    if (atlas->glyphs) {
        atlas->permanent.free(atlas->permanent.userdata, atlas->glyphs);
        atlas->glyphs = 0;
    }
    if (atlas->pixel) {
        atlas->temporary.free(atlas->temporary.userdata, atlas->pixel);
        atlas->pixel = 0;
    }
    return 0;
}

void usc_nk_sdl_font_stash_end(void)
{
    const void* image; int w, h;
    image = usc_nk_font_atlas_bake(&sdl.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    nk_sdl_device_upload_atlas(image, w, h);
    nk_font_atlas_end(&sdl.atlas, nk_handle_id((int)sdl.ogl.font_tex), &sdl.ogl.null);
    if (sdl.atlas.default_font)
        nk_style_set_font(&sdl.ctx, &sdl.atlas.default_font->handle);
}

const void* usc_nk_bake_atlas(nk_font_atlas* atlas, int& w, int& h)
{
    const void* image;
    image = usc_nk_font_atlas_bake(atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    return image;
}

NK_API void
nk_font_atlas_end_keep_atlas(struct nk_font_atlas *atlas, nk_handle texture,
    struct nk_draw_null_texture *null)
{
    int i = 0;
    struct nk_font *font_iter;
    NK_ASSERT(atlas);
    if (!atlas) {
        if (!null) return;
        null->texture = texture;
        null->uv = nk_vec2(0.5f,0.5f);
    }
    if (null) {
        null->texture = texture;
        null->uv.x = (atlas->custom.x + 0.5f)/(float)atlas->tex_width;
        null->uv.y = (atlas->custom.y + 0.5f)/(float)atlas->tex_height;
    }
    for (font_iter = atlas->fonts; font_iter; font_iter = font_iter->next) {
        font_iter->texture = texture;
#ifdef NK_INCLUDE_VERTEX_BUFFER_OUTPUT
        font_iter->handle.texture = texture;
#endif
    }
    for (i = 0; i < NK_CURSOR_COUNT; ++i)
        atlas->cursors[i].img.handle = texture;
}

NK_INTERN void
usc_nk_sdl_device_upload_atlas(const void *image, int width, int height)
{
#ifndef EMBEDDED
    // Use PDO for async texture upload
    GLuint pdo;
    glGenBuffers(1, &pdo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pdo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 4*(nk_size)width*(nk_size)height, image, GL_STATIC_DRAW);
#endif
    struct nk_sdl_device *dev = &sdl.ogl;
    glGenTextures(1, &dev->font_tex);
    glBindTexture(GL_TEXTURE_2D, dev->font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifndef EMBEDDED
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDeleteBuffers(1, &pdo);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image);
#endif
}

GLuint usc_nk_sdl_generate_texture(nk_font_atlas* atlas, const void* image, int w, int h)
{
    usc_nk_sdl_device_upload_atlas(image, w, h);
    nk_font_atlas_end_keep_atlas(atlas, nk_handle_id((int)sdl.ogl.font_tex), &sdl.ogl.null);
    atlas->temporary.free(atlas->temporary.userdata, atlas->pixel);
    atlas->pixel = nullptr;
    if (atlas->default_font)
        nk_style_set_font(&sdl.ctx, &atlas->default_font->handle);
    sdl.atlas = *atlas;
    return sdl.ogl.font_tex;
}

void usc_nk_sdl_use_atlas(nk_font_atlas* atlas, GLuint texture)
{
    NK_ASSERT(atlas);
    sdl.atlas = *atlas;
    sdl.ogl.font_tex = texture;

    nk_draw_null_texture* null = &sdl.ogl.null;
	null->texture = nk_handle_id((int)texture);
	null->uv.x = (sdl.atlas.custom.x + 0.5f)/(float)sdl.atlas.tex_width;
	null->uv.y = (sdl.atlas.custom.y + 0.5f)/(float)sdl.atlas.tex_height;
    if (sdl.atlas.default_font)
        nk_style_set_font(&sdl.ctx, &sdl.atlas.default_font->handle);
}

NK_API void
nk_sdl_device_destroy_keep_font(void)
{
    struct nk_sdl_device *dev = &sdl.ogl;
    glDetachShader(dev->prog, dev->vert_shdr);
    glDetachShader(dev->prog, dev->frag_shdr);
    glDeleteShader(dev->vert_shdr);
    glDeleteShader(dev->frag_shdr);
    glDeleteProgram(dev->prog);
    glDeleteBuffers(1, &dev->vbo);
    glDeleteBuffers(1, &dev->ebo);
    nk_buffer_free(&dev->cmds);
}

NK_API
void nk_sdl_shutdown_keep_font(void)
{
    nk_free(&sdl.ctx);
    nk_sdl_device_destroy_keep_font();
    memset(&sdl, 0, sizeof(sdl));
}

NK_API void
nk_atlas_font_stash_begin(struct nk_font_atlas *atlas)
{
    nk_font_atlas_init_default(atlas);
    nk_font_atlas_begin(atlas);
}
