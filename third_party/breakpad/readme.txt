To rebuild breakpad, you need windows depot_tools then you can do

fetch breakpad

Then you can rebuild the vsproject in src/src/client/windows/

!! IMPORTANT !!
Make sure the C/C++ > Code Generation > Runtime Library is /MD or
/MDd (if building debug) and that you are building x64 version

Then you can copy the common.lib, crash_generation_client.lib,
and exception_handler.lib from verious dirs into this dir

Now you should be able to link to the new build!
