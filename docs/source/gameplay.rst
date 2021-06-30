Gameplay
========
The following fields are available under the ``gameplay`` table:

.. code-block:: c#

    string title
    string artist
    string jacketPath
    bool demoMode
    int difficulty
    int level
    float progress // 0.0 at the start of a song, 1.0 at the end
    float hispeed
    int hispeedAdjust // 0 = not adjusting, 1 = coarse (xmod) adjustment, 2 = fine (mmod) adjustment
    float bpm
    Gauge gauge
    
    // The following are all in the range 0.0 - 1.0
    float hiddenCutoff
    float suddenCutoff
    float hiddenFade
    float suddenFade
    
    bool autoplay
    int comboState // 2 = puc, 1 = uc, 0 = normal
    bool[6] noteHeld // Array indicating wether a hold note is being held, in order: ABCDLR
    bool[2] laserActive // Array indicating if the laser cursor is on a laser, in order: LR
    ScoreReplay[] scoreReplays //Array of previous scores for the current song
    CritLine critLine // info about crit line and everything attached to it
    
    HitWindow hitWindow // This may be absent (== nil) for the default timing window (46 / 92 / 138 / 250ms)
    bool multiplayer
    // Multiplayer only, absent (== nil) in non-multiplay
    string user_id
    
    // Practice mode only, absent (== nil) in non-practice
    bool practice_setup // true: it's the setup, false: practicing now, nil: not in the practice mode
    
Example:    

.. code-block:: lua

    --Draw combo
    gfx.BeginPath()
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE)
    if gameplay.comboState == 2 then
        gfx.FillColor(100,255,0) --puc
    elseif gameplay.comboState == 1 then
        gfx.FillColor(255,200,0) --uc
    else
        gfx.FillColor(255,255,255) --regular
    end
    gfx.LoadSkinFont("NovaMono.ttf")
    gfx.FontSize(50 * math.max(comboScale, 1))
    comboScale = comboScale - deltaTime * 3
    gfx.Text(tostring(combo), posx, posy)

    
ScoreReplay
***********
A ``ScoreReplay`` contains the following fields:
    
.. code-block:: c

    int maxScore //the final score of this replay
    int currentScore //the current score of this replay

    
CritLine
********
A ``CritLine`` contains the following fields:
    
.. code-block:: c

    int x //the x screen coordinate of the center of the critical line
    int y //the y screen coordinate of the center of the critical line
    float rotation //the rotation of the critical line in radians
    Cursor[] cursors //the laser cursors, indexed 0 and 1 for left and right
    Line line // Line going from the left corner of the track to the right

    
Cursor
******
A ``Cursor`` contains the following fields:
    
.. code-block:: c

    float pos //the x position relative to the center of the crit line
    float alpha //the transparency of this cursor. 0 is transparent, 1 is opaque
    float skew //the x skew of this cursor to simulate a more 3d look
    
Line
****
A ``Line`` contains the following fields:

.. code-block:: c
    
    float x1 // start x coordinate
    float y1 // start y coordinate
    float x2 // end x coordinate
    float y2 // end y coordinate

Gauge
*****
A ``Gauge`` contains the following fields:

.. code-block:: c
    
    int type // 0 = normal, 1 = hard. Should be defined in constants sometime
    int options // type specific, such as difficulty level for the same gauge type if available
    float value // current gauge value
    char* name


HitWindow
*********
A ``HitWindow`` contains the following fields:

.. code-block:: c

    int type // 0: expand-judge, 1: normal, 2: hard
    int perfect
    int good
    int hold
    int miss

Calls made to lua
*****************
These are functions the game calls in the gameplay lua script so they need to be defined in there. The reason for having these is mostly for updating and starting animations.

update_score(newScore)
^^^^^^^^^^^^^^^^^^^^^^
For updating the score in lua.

update_combo(newCombo)
^^^^^^^^^^^^^^^^^^^^^^
For updating the combo in lua.

near_hit(wasLate)
^^^^^^^^^^^^^^^^^
For updating early/late display.

button_hit(button, rating, delta)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Can be used for a number of things, such as starting custom hit animations or more advanced early/late displays.
``button`` uses the same values as the ``game.BUTTON_*`` values.
``delta`` is the hit time from perfect, positive values = late, negative values = early.

``rating`` is the hit rating and the values are:

.. code-block:: c

    0 = Miss
    1 = Near
    2 = Crit
    3 = Idle

Idle and Miss are special cases that do not have any delta (delta always 0). Idle is triggered when the player
hits the button when there is no note object in range on that lane.

laser_slam_hit(slamLength, startPos, endPost, index)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
For animating laser slam hits.
``slamLength`` is the length between slams from -2.0 through 2.0. The sign on this value indicates the slam direction.
``startPos`` is the x offset from the center of the crit line where the slam starts
``endPos`` is the x offset from the center of the crit line where the slam ends
``index`` indicates which laser the slam was for

laser_alert(isRight)
^^^^^^^^^^^^^^^^^^^^
For starting laser alert animations::

    if isRight == true then restart right alert animation
    else restart left alert animation
    
render(deltaTime)
^^^^^^^^^^^^^^^^^
The GUI render call. This is called last and will draw over everything else.
    
render_crit_base(deltaTime)
^^^^^^^^^^^^^^^^^^^^^^^^^^^
Function to render the base of the critical line. This function will be called
after rendering the highway and playable objects, but before the built-in particle
effects. Use this to draw the critical line itself as well as the darkening effects
placed over the playable objects.

See the default skin for an example.
    
render_crit_overlay(deltaTime)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Function to render the rest of the critical line, this is the last thing to be called
before ``render`` so anything else which belongs above the built-in particle effects goes here.
This is the place to draw the laser cursors.

See the default skin for an example.
    
render_intro(deltaTime)
^^^^^^^^^^^^^^^^^^^^^^^
Function for rendering an intro or keeping an intro timer. This function will be
called every frame until it returns ``true`` and never again after it has.

Example:

.. code-block:: lua

    render_intro = function(deltaTime)
        if not game.GetButton(game.BUTTON_STA) then
            introTimer = introTimer - deltaTime
        end
        introTimer = math.max(introTimer, 0)
        return introTimer <= 0
    end

render_outro(deltaTime, clearState)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Function for rendering an outro or keeping an outro timer.

This function can return two values, the first being a boolean to tell the game
when the outro has completed and the second must be a number that sets the playback
speed, like so:

.. code-block:: lua
    
    local outroTimer = 0
    --Slows the playback to a stop for the first second
    --and then goes to the result screen after another second
    render_outro = function(deltaTime, clearState)
        outroTimer = outroTimer + deltaTime --counts timer up
        return outroTimer > 2, 1 - outroTimer
    end


This function gets called when the game has ended till the game has transitioned into
the result screen, the game starts transitioning when this function returns ``true``
for the first time.

``clearState`` tells this function if the player failed or cleared the game for example.
These are all the possible states::

    0 = Player manually exited the game
    1 = Failed
    2 = Cleared
    3 = Hard Cleared
    4 = Full Combo
    5 = Perfect

practice_start(mission_type, mission_threshold, mission_description)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
(Practice mode only) This is called when the practice is being started.
``mission_type`` is the current mission type (one of None, Score, Grade, Miss, MissAndNear, and Gauge).
``mission_threshold`` is the parameter value for the current mission.
``mission_description`` is a textual description for the current mission, and is suitable for displaying.

practice_end_run(playCount, successCount, isSuccessful, scoring)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
(Practice mode only) This is called when a run ("one loop") is finished.
``playCount``, ``successCount``, ``isSuccessful`` are self-explationary.
``scoring`` is a table containing informations on the current run with the following fields.

.. code-block:: c

    int score
    int perfects
    int goods
    int misses
    int medianHitDelta
    int meanHitDelta
    int medianHitDeltaAbs
    int meanHitDeltaAbs

``score`` changes depending on current score display setting.

practice_end(playCount, successCount)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
(Practice mode only) This is called when the practice setup is entered again after practicing.