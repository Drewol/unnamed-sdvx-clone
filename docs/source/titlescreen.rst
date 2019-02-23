Title Screen
==============
The screen the game boots into.

Contains ``menu`` which has the following functions:

.. code-block:: c#

    // Goes to the song select screen
    void SongSelect()

    // Goes to the settings screen
    void Settings()

    // Exits the game
    void Exit()

Calls made to lua
*****************
This screen makes one call to lua:

.. code-block:: c#
    
    // called whenever the user pressed a mouse button
    void mouse_pressed(int32 button) 
    