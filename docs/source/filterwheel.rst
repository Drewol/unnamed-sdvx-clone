Filter Wheel
============
Wheel for selecting which filters to use to filter the song selection.

Contains ``filters`` which has the following fields:

.. code-block:: c#

    string[] level //Contains all level filters
    string[] foler //Contains all folder filters

Calls made to lua
*****************
This screen makes two calls to lua:

.. code-block:: c#

    //called whenever the user switches between selecting folder filters and level filters
    void set_mode(bool selectingFolders)

    //called whenever the user scrolls, selectingFolders tells which arrays index to update
    void set_selection(int index, bool selectingFolders)