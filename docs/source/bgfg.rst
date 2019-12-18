Background & Foreground
=======================
How to use backgrounds and foregrounds and the functions available in the lua tables for
background and foreground scritps.

Backgrounds and foregrounds work exactly the same with the exception that the foreground shader gets
access to a texture built from the current framebuffer in the ``fb_tex`` uniform. The ``background`` and
``foreground`` scripts access their functions in the tables named as such but the functions are identical
for both tables.

To create a background you need to create a folder containing the following files::

    bg.fs
    bg.lua

For a foreground you need to create the following files in the same folder::

    fg.fs
    fg.lua

The ``.fs`` files do not need to do anything but they need to exist and they need to be valid glsl
fragment shaders.

If a background or foreground fails to load the game will simply not draw the background or foreground
and will continue on as usual without them so that the game is still playable.

The backgrounds that are available in ksh are::

    arrow
    sakura
    smoke
    snow
    techno
    wave
    hidden

You should have all these in your skin (excluding hidden) so that all backgrounds that can appear in ksh
has something that will be displayed.

In the following documentation only "background" or "bg" will be used but everything applies to both
backgrounds and foregrounds.

To create a chart provided background you simply just have to create a background as specified above in the
same folder as your ksh and in the ksh you have to change the ``layer=`` value to the name of the folder, a
chart provided background will not override any of the default background so it needs to have a name that
does not collide with the default ones.

Calls made to lua
*****************

render_xx(deltaTime)
^^^^^^^^^^^^^^^^^^^^
Here is where you draw the background or foreground. The ``xx`` is ``bg`` in backgrounds and ``fg`` in foregrounds.

Available functions
*******************
The following functions are available in the ``background`` and ``foreground`` tables.

LoadTexture(string shadername, string filename)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Loads a texture and makes it available in the fragment shader under the given ``shadername``.
This function should be called outside any function ``render_xx`` function so it only executes once and during
loading.

SetParami(string name, int value)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Set an integer value to a uniform variable in the shader.

SetParamf(string name, float value)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Set a float value to a uniform variable in the shader.

DrawShader()
^^^^^^^^^^^^
Draws the shader and also invokes a ``gfx.ForceRender()`` call.

GetPath()
^^^^^^^^^
Retrieves the path to the background, necessary for loading textures with regular ``gfx`` functions.

SetSpeedMult(float speed)
^^^^^^^^^^^^^^^^^^^^^^^^^
Sets a speed multiplier for the offsync timer in the ``timing``.

GetTiming()
^^^^^^^^^^^
Returns timing data in 3 parts ``(bartime, offsync, real)``::

    bartime: goes from 0 to 1 over the duration of a beat then resets to 0.
    offsync: goes from 0 to 1 over a duration decided by bpm and multiplier then resets to 0
    real: current time in the chart. Can be used to time background events with the music.


GetTilt()
^^^^^^^^^
Retrieves a pair of tilt values: ``(laser, spin)``

``laser`` is the tilt amount induced by lasers.
``spin`` is the tilt amount induced by spin events.

GetScreenCenter()
^^^^^^^^^^^^^^^^^
Retrieves the screen center as pixel coordinates ``(x,y)``. Note that this is not the actual screen center
but a point slightly above the end of the track that would be closer to a "vanishing point" but also not
exactly that.

GetClearTransition()
^^^^^^^^^^^^^^^^^^^^
Returns a value that goes from 0 to 1 when the player reaches a clear state or from 1 to 0 when the player
goes from a clear state to a fail state. The transition happens over the duration of one beat and will not
jump if the state changes mid transition.