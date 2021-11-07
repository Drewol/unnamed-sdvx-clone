Drawing
========
This chapter describes all the functions available under the ``gfx``
table. Most functions work exactly like their nanovg counterpart.

Example usage

.. code-block:: lua

    --Draw score
    gfx.BeginPath()
    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.Translate(-5,5) -- upper right margin
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_TOP)
    gfx.FontSize(40)
    gfx.Text(string.format("%08d", score),desw,0)

    
Constants
*************************************
Most constants are from the nanovg library.

+------------------------+-------------------------------------------+
|    Lua Name            |         Value                             |
+========================+===========================================+
| TEXT_ALIGN_BASELINE    | NVGalign::NVG_ALIGN_BASELINE              |
+------------------------+-------------------------------------------+
| TEXT_ALIGN_BOTTOM      | NVGalign::NVG_ALIGN_BOTTOM                |
+------------------------+-------------------------------------------+
| TEXT_ALIGN_CENTER      | NVGalign::NVG_ALIGN_CENTER                |
+------------------------+-------------------------------------------+
| TEXT_ALIGN_LEFT        | NVGalign::NVG_ALIGN_LEFT                  |
+------------------------+-------------------------------------------+
| TEXT_ALIGN_MIDDLE      | NVGalign::NVG_ALIGN_MIDDLE                |
+------------------------+-------------------------------------------+
| TEXT_ALIGN_RIGHT       | NVGalign::NVG_ALIGN_RIGHT                 |
+------------------------+-------------------------------------------+
| TEXT_ALIGN_TOP         | NVGalign::NVG_ALIGN_TOP                   |
+------------------------+-------------------------------------------+
| LINE_BEVEL             | NVGlineCap::NVG_BEVEL                     |
+------------------------+-------------------------------------------+
| LINE_BUTT              | NVGlineCap::NVG_BUTT                      |
+------------------------+-------------------------------------------+
| LINE_MITER             | NVGlineCap::NVG_MITER                     |
+------------------------+-------------------------------------------+
| LINE_ROUND             | NVGlineCap::NVG_ROUND                     |
+------------------------+-------------------------------------------+
| LINE_SQUARE            | NVGlineCap::NVG_SQUARE                    |
+------------------------+-------------------------------------------+
| IMAGE_GENERATE_MIPMAPS | NVGimageFlags::NVG_IMAGE_GENERATE_MIPMAPS |
+------------------------+-------------------------------------------+
| IMAGE_REPEATX          | NVGimageFlags::NVG_IMAGE_REPEATX          |
+------------------------+-------------------------------------------+
| IMAGE_REPEATY          | NVGimageFlags::NVG_IMAGE_REPEATY          |
+------------------------+-------------------------------------------+
| IMAGE_FLIPY            | NVGimageFlags::NVG_IMAGE_FLIPY            |
+------------------------+-------------------------------------------+
| IMAGE_PREMULTIPLIED    | NVGimageFlags::NVG_IMAGE_PREMULTIPLIED    |
+------------------------+-------------------------------------------+
| IMAGE_NEAREST          | NVGimageFlags::NVG_IMAGE_NEAREST          |
+------------------------+-------------------------------------------+


BeginPath()
******************************************************
nanovg.h:461_

.. _nanovg.h:461: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L461

Rect(float x, float y, float w, float h)
******************************************************
nanovg.h:491_

.. _nanovg.h:491: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L491

FastRect(float x, float y, float w, float h)
******************************************************

Fill()
******************************************************
nanovg.h:506_

.. _nanovg.h:506: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L506

FillColor(int r, int g, int b, int a = 255)
******************************************************
nanovg.h:251_

.. _nanovg.h:251: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L251

CreateImage(const char* filename, int imageflags)
******************************************************
nanovg.h:372_

.. _nanovg.h:372: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L372

CreateSkinImage(const char* filename, int imageflags)
******************************************************
Same as CreateImage but prepends ``"skins/[skinfolder]/textures/"`` to the filename.


ImageRect(float x, float y, float w, float h, int image, float alpha, float angle)
**********************************************************************************
Draws an image in the specified rect stretching the image to fit.


Text(const char* s, float x, float y)
******************************************************
nanovg.h:584_

.. _nanovg.h:584: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L584

TextAlign(int align)
******************************************************
nanovg.h:575_

.. _nanovg.h:575: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L575

FontFace(const char* s)
******************************************************
nanovg.h:581_

.. _nanovg.h:581: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L581

FontSize(float size)
******************************************************
nanovg.h:563_

.. _nanovg.h:563: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L563

Translate(float x, float y)
******************************************************
nanovg.h:303_

.. _nanovg.h:303: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L303

Scale(float x, float y)
******************************************************
nanovg.h:315_

.. _nanovg.h:315: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L315

Rotate(float angle)
******************************************************
nanovg.h:306_

.. _nanovg.h:306: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L306

ResetTransform()
******************************************************
nanovg.h:293_

.. _nanovg.h:293: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L293

LoadFont(const char* name, const char* filename)
******************************************************
Loads a font and sets it as the current font. Only sets
the current font if the font is already loaded.

LoadSkinFont(const char* name, const char* filename)
******************************************************
Same as LoadFont but prepends ``"skins/[skinfolder]/fonts/"`` to the filename.

FastText(const char* inputText, float x, float y)
**********************************************************************
A text rendering function that is slightly faster than the regular ``gfx.Text()``
but this text will always be drawn on top of any nanovg drawing.

CreateLabel(const char* text, int size, bool monospace)
*******************************************************
Creates a cached text that can later be drawn with ``DrawLabel``. This is the most
resource efficient text drawing method.

DrawLabel(int labelId, float x, float y, float maxWidth = -1)
**************************************************************
Renders an already created label.

Will resize the label to fit within the maxWidth if maxWidth > 0.

MoveTo(float x, float y)
******************************************************
nanovg.h:465_

.. _nanovg.h:465: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L465

LineTo(float x, float y)
******************************************************
nanovg.h:468_

.. _nanovg.h:468: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L468

BezierTo(float c1x, float c1y, float c2x, float c2y, float x, float y)
**********************************************************************
nanovg.h:471_

.. _nanovg.h:471: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L471

QuadTo(float cx, float cy, float x, float y)
******************************************************
nanovg.h:474_

.. _nanovg.h:474: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L474

ArcTo(float x1, float y1, float x2, float y2, float radius)
***********************************************************
nanovg.h:477_

.. _nanovg.h:477: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L477

ClosePath()
******************************************************
nanovg.h:480_

.. _nanovg.h:480: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L480

MiterLimit(float limit)
******************************************************
nanovg.h:258_

.. _nanovg.h:258: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L258

StrokeWidth(float size)
******************************************************
nanovg.h:261_

.. _nanovg.h:261: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L261

LineCap(int cap)
******************************************************
nanovg.h:265_

.. _nanovg.h:265: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L265

LineJoin(int join)
******************************************************
nanovg.h:269_

.. _nanovg.h:269: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L269

Stroke()
******************************************************
nanovg.h:509_

.. _nanovg.h:509: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L509

StrokeColor(int r, int g, int b, int a = 255)
******************************************************
nanovg.h:245_

.. _nanovg.h:245: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L245

UpdateLabel(int labelId, const char* text, int size)
******************************************************
Updates an already existing label.

DrawGauge(float rate, float x, float y, float w, float h, float deltaTime)
**************************************************************************
Draws the currently loaded gauge, the game loads the gauge on its own.

SetGaugeColor(int colorindex, int r, int g, int b)
***************************************************
Sets the gauge color for the specified ``colorindex``. The color indexes are::

    0 = Normal gauge fail
    1 = Normal gauge clear
    2 = Hard gauge low (<30%)
    3 = Hard gauge high (>30%)

Example:

.. code-block:: lua

    gfx.SetGaugeColor(0,50,50,50) --Normal gauge fail = dark grey
    gfx.SetGaugeColor(1,255,255,255) --Normal gauge clear = white
    gfx.SetGaugeColor(2,50,0,0) --Hard gauge low (<30%) = dark red
    gfx.SetGaugeColor(3,255,0,0) --Hard gauge high (>30%) = red

RoundedRect(float x, float y, float w, float h, float r)
**************************************************************************
nanovg.h:494_

.. _nanovg.h:494: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L494

RoundedRectVarying(float x, float y, float w, float h, float radTopLeft, float radTopRight, float radBottomRight, float radBottomLeft)
**************************************************************************************************************************************
nanovg.h:497_

.. _nanovg.h:497: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L497

Ellipse(float cx, float cy, float rx, float ry)
**************************************************************************
nanovg.h:500_

.. _nanovg.h:500: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L500

Circle(float cx, float cy, float r)
**************************************************************************
nanovg.h:503_

.. _nanovg.h:503: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L503

SkewX(float angle)
**************************************************************************
nanovg.h:309_

.. _nanovg.h:309: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L309

SkewY(float angle)
**************************************************************************
nanovg.h:312_

.. _nanovg.h:312: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L312

LinearGradient(float sx, float sy, float ex, float ey)
**************************************************************************
nanovg.h:400_

.. _nanovg.h:400: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L400

BoxGradient(float x, float y, float w, float h, float r, float f)
**************************************************************************
nanovg.h:408_

.. _nanovg.h:408: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L408

RadialGradient(float cx, float cy, float inr, float outr)
**************************************************************************
nanovg.h:414_

.. _nanovg.h:414: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L414

ImagePattern(float ox, float oy, float ex, float ey, float angle, int image, float alpha)
******************************************************************************************
nanovg.h:420_

.. _nanovg.h:420: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L420

UpdateImagePattern(int paint, float ox, float oy, float ex, float ey, float angle, float alpha)
***********************************************************************************************
Updates the parameters of an already existing ImagePattern.


GradientColors(int ri, int gi, int bi, int ai, int ro, int go, int bo, int ao)
*******************************************************************************
Sets icol (inner color) and ocol (outer color) for the gradient functions.

FillPaint(int paint)
**************************************************************************
nanovg.h:254_

.. _nanovg.h:254: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L254

StrokePaint(int paint)
**************************************************************************
nanovg.h:248_

.. _nanovg.h:248: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L248

Save()
*******
nanovg.h:224_

.. _nanovg.h:224: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L224

Restore()
**********
nanovg.h:227_

.. _nanovg.h:227: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L227

Reset()
********
nanovg.h:230_

.. _nanovg.h:230: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L230

PathWinding(int dir)
*********************
nanovg.h:483_

.. _nanovg.h:483: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L483

ForceRender()
**************
Forces the current render queue to be processed which makes it possible to put any
Fast\* and Label drawing calls made before a ForceRender call under regular drawing
functions called after a ForceRender call.

This function might have a more than insignificant performance impact.
under regular 

LoadImageJob(char* path, int placeholder, int w = 0, int h = 0)
****************************************************************
Loads an image outside the main thread to not lock up the rendering. If ``w`` and ``h``
are greater than 0 then the image will be resized if it is larger than ``(w,h)``, if
w or h are 0 the image will be loaded at its original size.

Returns ``placeholder`` until the image has been loaded.

Example:

.. code-block:: lua

    if not songCache[song.id][selectedDiff] or songCache[song.id][selectedDiff] == jacketFallback then
        songCache[song.id][selectedDiff] = gfx.LoadImageJob(diff.jacketPath, jacketFallback, 200,200)
    end
    
    
LoadWebImageJob(char* url, int placeholder, int w = 0, int h = 0)
*****************************************************************
Works the same as ``LoadImageJob`` but the path is a web url instead.
   
Scissor(float x, float y, float w, float h)
****************************************************************
nanovg.h:431_

.. _nanovg.h:431: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L431

IntersectScissor(float x, float y, float w, float h)
****************************************************************
nanovg.h:439_

.. _nanovg.h:439: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L439

ResetScissor()
****************************************************************
nanovg.h:442_

.. _nanovg.h:442: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L442

TextBounds(float x, float y, char* s)
*****************************************
Returns ``xmin,ymin, xmax,ymax`` for a ``gfx.Text`` text.

LabelSize(int label)
*********************
Returns ``w,h`` for a label created with ``gfx.CreateLabel``

FastTextSize(char* text)
*************************
Returns ``w,h`` for a ``gfx.FastText`` text.

ImageSize(int image)
********************
Returns ``w,h`` for the given image.

Arc(float cx, float cy, float r, float a0, float a1, int dir)
*************************************************************
nanovg.h:488_

.. _nanovg.h:488: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L488

SetImageTint(int r, int g, int b)
*********************************
Sets the color to tint all coming image drawing calls with.
(Multiplies the color given with the image colors)


GlobalCompositeOperation(int op)
********************************
nanovg.h:171_

The available ``op`` are:

.. code-block:: lua

	gfx.BLEND_OP_SOURCE_OVER <--- default
	gfx.BLEND_OP_SOURCE_IN
	gfx.BLEND_OP_SOURCE_OUT
	gfx.BLEND_OP_ATOP
	gfx.BLEND_OP_DESTINATION_OVER
	gfx.BLEND_OP_DESTINATION_IN
	gfx.BLEND_OP_DESTINATION_OUT
	gfx.BLEND_OP_DESTINATION_ATOP
	gfx.BLEND_OP_LIGHTER
	gfx.BLEND_OP_COPY
	gfx.BLEND_OP_XOR


.. _nanovg.h:171: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L171


GlobalCompositeBlendFunc(int sfactor, int dfactor)
**************************************************
nanovg.h:174_

The available blend factors are:

.. code-block:: c

	gfx.BLEND_ZERO = 1<<0
	gfx.BLEND_ONE = 1<<1
	gfx.BLEND_SRC_COLOR = 1<<2
	gfx.BLEND_ONE_MINUS_SRC_COLOR = 1<<3
	gfx.BLEND_DST_COLOR = 1<<4
	gfx.BLEND_ONE_MINUS_DST_COLOR = 1<<5
	gfx.BLEND_SRC_ALPHA = 1<<6
	gfx.BLEND_ONE_MINUS_SRC_ALPHA = 1<<7
	gfx.BLEND_DST_ALPHA = 1<<8
	gfx.BLEND_ONE_MINUS_DST_ALPHA = 1<<9
	gfx.BLEND_SRC_ALPHA_SATURATE = 1<<10


.. _nanovg.h:174: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L174


GlobalCompositeBlendFuncSeparate(int srcRGB, int dstRGB, int srcAlpha, int dstAlpha)
************************************************************************************
nanovg.h:177_

Uses the same parameter values as ``GlobalCompositeBlendFunc``

.. _nanovg.h:177: https://github.com/memononen/nanovg/blob/master/src/nanovg.h#L177

LoadAnimation(char* path, float frametime, int loopcount = 0, bool compressed = false)
**************************************************************************************
Loads all the images in a specified folder as an animation. ``frametime`` is used for the speed of the animation
and if ``loopcount`` is set to something that isn't 0 then the animation will stop after playing that many times.

If ``compressed`` is set to true then the animation will be stored in memory in a compressed format and each frame
will be decoded on-demand which means that the animation uses much less RAM but it uses more CPU and the animation
might stutter or skip frames if it's too heavy.

Returns a numer that is used the same way a regular image is used.

GlobalAlpha(float alpha)
************************
Sets a global alpha value for all nanovg drawing. Already transparent objects will have their tranparency
adjusted relative to the global value.

LoadSkinAnimation(char* path, float frametime, int loopcount = 0, bool compressed = false)
******************************************************************************************
Same as LoadAnimation but prepends ``"skins/[skinfolder]/textures/"`` to the path.

TickAnimation(int animation, float deltaTime)
*********************************************
Progresses the given animation. Returns -1 if animation not found, 0 if the animation is still loading, and 1 if the animation is loaded.

LoadSharedTexture(string key, string path)
******************************************
Load a persistent texture that can be accessed by the ``key`` in ``GetSharedTexture`` and ``ShadedMesh:AddSharedTexture``

LoadSharedSkinTexture(string key, string path)
**********************************************
Same as LoadSharedTexture but prepends ``"skins/[skinfolder]/textures/"`` to the path.

GetSharedTexture(string key)
****************************
Returns the shared texture ID generated by ``LoadSharedTexture`` which is used the same as images loaded with ``CreateImage``.
