Track
=====
Functions available under the `track` table, these are only available in `gameplay.lua` and background/foreground scripts.

track.HideObject(int time, int lane)
************************************
Hide a hit object at a given time (in miliseconds) and in a given lane. This will prevent the hit object from rendering. This only works for buttons currently. Lanes are::

    1: A Button
    2: B Button
    3: C Button
    4: D Button
    5: Left FX
    6: Right FX

Note: This will try to find the closest object at or after the given time.

track.GetCurrentLaneXPos(int lane)
**********************************
Get the x value for the left side of the given lane (see above for lanes). This value should always be static unless there is center-split.

track.GetYPosForTime(int time)
******************************
Get the y position an object would be at for a given time (in miliseconds) in the song. This adjusts for current track speed, so it can be used for moving a mesh along the track at the correct location for a given time.

track.GetLengthForDuration(int start, int duration)
***************************************************
Get the y length of a long note that starts at a given time (in miliseconds) and goes for a given duration (in miliseconds). This adjusts for the current track speed.

track.CreateShadedMeshOnTrack(string material = "guiTex")
*******************************
Creates a new `ShadedMeshOnTrack` object, the material is loaded from the skin shaders folder where
``material.fs`` and ``material.vs`` need to exist. See ShadedMeshOnTrack for more information.

