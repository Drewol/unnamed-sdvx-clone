Globals
==============
Anything globaly available in Lua

include(char* path)
*********
Loads a Lua file into the current environment.
Included files may return values; if so, these values are returned from include.
The path parameter is expected to the a normal path relative to the skin folder, without
the .lua file extension.

It's good practice in many cases to return a table containing the values and functions
you would like to export to someone who includes the script.
See the ini.lua file in the default script.

Included files are re-loaded every time they are include.
USC is very library with reloading scripts, so include avoids caching anything.

.. code-block:: c#

   // expands to "skins/Default/helper_code/my_cool_library.lua" for the Default skin
   // my_cool_library returns a table of its exported functions, we give that table a name
   my_cool_library = include "helper_code/my_cool_library"
