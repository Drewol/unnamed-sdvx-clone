Result Screen
=============
The following fields are available under the ``result`` table.
Note that, for multiplayer play every fields other than ``isSelf`` and ``uid`` may not be set to the viewer's.

.. code-block:: c#

    int score
    int gauge_type // 0 = normal, 1 = hard. Should be defined in constants sometime
    int gauge_option // type specific, such as difficulty level for the same gauge type if available    
    bool mirror
    bool random
    int auto_flags //bits for autoplay settings, 0 = no autoplay
    float gauge // value of the gauge at the end of the song
    int misses
    int goods
    int perfects
    int maxCombo
    int level
    int difficulty
    string title // With the player name in multiplayer
    string realTitle // Always without the player name
    string artist
    string effector
    string illustrator
    string bpm
    int duration // Length of the chart in milliseconds
    string jacketPath
    int medianHitDelta
    float meanHitDelta
    int medianHitDeltaAbs
    float meanHitDeltaAbs
    int earlies
    int lates
    int badge // same as song wheel badge (except 0 which means the user manually exited)
    float gaugeSamples[256] // gauge values sampled throughout the song
    string grade // "S", "AAA+", "AAA", etc.
    score[] highScores // Same as song wheel scores
    string playerName // Only on multiplayer
    int displayIndex // Only on multiplayer; which player's score (not necessarily the viewer's) is being shown right not
    string uid // Only on multiplayer; the UID of the viewer
    HitWindow hitWindow // Same as gameplay HitWindow
    bool autoplay
    float playbackSpeed
    string mission // Only on practice mode
    int retryCount // Only on practice mode
    bool isSelf // Whether this score is viewer's in multiplayer; always true for singleplayer
    int speedModType // Only when isSelf is true; 0 for XMOD, 1 for MMOD, 2 for CMOD
    int speedModValue // Only when isSelf is true; HiSpeed for XMOD, ModSpeed for MMOD and CMOD
    HidSud hidsud // Only when isSelf is true
    HitStat[] noteHitStats // Only when isSelf is true; contains HitStat for notes (excluding hold notes and lasers) 
    HitStat[] holdHitStats // Only when isSelf is true; contains HitStat for holds
    HitStat[] laserHitStats // Only when isSelf is true; contains HitStat for lasers

HitStat
*******
A ``HitStat`` contains the following fields:
    
.. code-block:: c

    int rating // 0 for miss, 1 for near, 2 for crit
    int lane  // 0-3 btn, 4-5 fx, 6-7 lasers
    int time // In milliseconds
    float timeFrac // Between 0 and 1
    int delta
    int hold // 0 for chip or laser, otherwise # of ticks in hold


Calls made to lua
*****************
Calls made from the game to the script, these need to be defined for the game
to function properly.

result_set()
^^^^^^^^^^^^
This is called right after ``result`` is set, either for initial display or when the player whose score is being displayed is changed.

render(deltaTime)
^^^^^^^^^^^^^^^^^
The GUI render call.

get_capture_rect()
^^^^^^^^^^^^^^^^^^
The region of the screen to be saved in score screenshots.

Has to return ``x,y,w,h`` in pixel coordinates to the game.

screenshot_captured(path)
^^^^^^^^^^^^^^^^^^^^^^^^^
Called when a screenshot has been captured successfully with ``path`` being the
path to the saved screenshot.