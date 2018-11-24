Game
====
Functions available under the ``game`` table, these are available in every script.

Constants
*********

+--------------------+------------------------------+
|    Lua Name        |         Value                |
+====================+==============================+
|LOGGER_INFO         | Logger::Severity::Info       |
+--------------------+------------------------------+
|LOGGER_NORMAL       | Logger::Severity::Normal     |
+--------------------+------------------------------+
|LOGGER_WARNING      | Logger::Severity::Warning    |
+--------------------+------------------------------+
|LOGGER_ERROR        | Logger::Severity::Error      |
+--------------------+------------------------------+
|BUTTON_BTA          | Input::Button::BT_0          |
+--------------------+------------------------------+
|BUTTON_BTB          | Input::Button::BT_1          |
+--------------------+------------------------------+
|BUTTON_BTC          | Input::Button::BT_2          |
+--------------------+------------------------------+
|BUTTON_BTD          | Input::Button::BT_3          |
+--------------------+------------------------------+
|BUTTON_FXL          | Input::Button::FX_0          |
+--------------------+------------------------------+
|BUTTON_FXR          | Input::Button::FX_1          |
+--------------------+------------------------------+
|BUTTON_STA          | Input::Button::BT_S          |
+--------------------+------------------------------+


GetMousePos()
*************
Returns the mouse's position on the game window in pixel coordinates.

Example::

    mposx,mposy = game.GetMousePos();


GetResolution()
***************
Returns the game window's resolution in pixels.

Example::

    resx,resy = game.GetResolution();


Log(char* message, int severity)
********************************
Logs a message to the game's log file.

Example::

    game.Log("Something went wrong!", game.LOGGER_ERROR)
    

LoadSkinSample(char* name)
********************************
Loads a .wav sample from the audio directory in the current skin folder.

.wav should not be included in *name*.


PlaySample(char* name, bool loop = false)
*******************************************
Plays a loaded sample. Will loop the sample if loop == true.


StopSample(char* name)
*******************************************
Stop playing a sample instantly.


GetLaserColor(int laser)
************************
0 = left, 1 = right

returns r,g,b

GetButton(int button)
*********************
Returns the state of the specified button. Returns true if the button is pressed.

GetKnob(int knob)
*****************
Returns the absolute rotation of a knob with the parameter ``knob`` being ``0 = Left``
and ``1 = Right``.

The returned value ranges from 0 to 2*pi.