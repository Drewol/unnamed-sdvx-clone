
Song Wheel
============
Contains a list of songs accessed by ``songwheel.songs``

The list of all (not filtered) songs is available in ``songwheel.allSongs``

The current song database status is available in ``songwheel.searchStatus``

Example for loading the jacket of the first diff for every song:

.. code-block:: lua

    for i,song in ipairs(songwheel.songs) do
        jackets[song.id] = gfx.CreateImage(song.difficulties[1].jacketPath, 0)
    end

Song
***************
A song contains the following fields:


.. code-block:: c#

    string title
    string artist
    string bpm //ex. "170-200"
    int id //unique static identifier
    string path //folder the song is stored in
    difficulty[] difficulties //array of all difficulties for this song
    string searchText //current string used by the song search
    bool searchInputActive //true when the user is currently inputting search text

Difficulty
**********
A difficulty contains the following fields:


.. code-block:: c#

    string jacketPath
    int level
    int difficulty // 0 = nov, 1 = adv, etc.
    int id //unique static identifier
    string effector
    int bestBadge //top badge for this difficulty
    difficulty[] scores //array of all scores on this diff


Score
*****
A score contains the following fields:


.. code-block:: c#

    float gauge //range 0.0 -> 1.0
    int flags //contains information about gaugeType/random/mirror
    int score
    int perfects
    int goods
    int misses
    int badge
    int timestamp //timestamp in POSIX time (seconds since Jan 1 1970 00:00:00 UTC)

Badge
*****
Values::

    0 = No badge/Never played
    1 = Played but not cleared
    2 = Cleared
    3 = Hard Cleared
    4 = Full Combo
    5 = Perfect


get_page_size
*************
Function called by the game to get how much to scroll when page up or page down are pressed.
Needs to be defined for the game to work properly.

songs_changed(withAll)
**********************
Function called by the game when ``songs`` or ``allSongs`` (if withAll == true) is changed.
