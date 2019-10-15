Gameplay background
===================

Calls made to lua
*****************
These are functions the game calls in the game/background script. None of them are required to implement but they do expose extra control over the game's background.

get_bg_file()
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Function to render a different default background image file depending on conditions (e.g. difficulty, skin config, randomness).
Should return a string that points to an image inside the ``textures`` folder (e.g. returning "backgrounds/bg1.png" will point to "textures/backgrounds/bg1.png").

(If you don't include this function it will use "bg_texture.png" by default)

get_fg_file()
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Exactly the same as get_bg_file(), except this is for the foreground image during gameplay.
(If you don't include this function it will use "fg_texture.png" by default)
