ShadedMesh
==========
ShadedMesh is a lua object available that has "low-level" drawing capabilites that can be useful
for very custom rendering goals.

Example drawing a green rectangle in additive mode:

.. code-block:: lua

    local testMesh = gfx.CreateShadedMesh("guiColor")
    testMesh:SetParamVec4("color", 0.0, 0.7, 0.0, 1.0)
    testMesh:SetPrimitiveType(testMesh.PRIM_TRIFAN)
    testMesh:SetBlendMode(testMesh.BLEND_ADD)
    testMesh:SetData({
        {{0,0},{0,0}}, 
        {{50,0},{1,0}}, 
        {{50,50},{1,1}}, 
        {{0,50},{0,1}}, 
    })
    
    function render(deltaTime)
        testMesh:Draw()
    end
    
    
gfx.CreateShadedMesh(String material = "guiTex")
************************************************
Creates a new ShadedMesh object, the material is loaded from the skin shaders folder where
``material.fs`` and ``material.vs`` need to exist.

gfx.CreateShadedMesh(String material, String path)
**************************************************
Creates a new ShadedMesh object, the material is loaded from the given path where
``material.fs`` and ``material.vs`` need to exist.

ShadedMesh:Draw()
*****************
Renders the ShadedMesh object.

ShadedMesh:AddTexture(String name, String path)
***********************************************
Adds a texture to the material that can be used in the shader code.

ShadedMesh:AddSkinTexture(String name, String path)
***************************************************
Same as AddTexture but it looks for the path using the skin texture folder as the root folder.

ShadedMesh:AddSharedTexture(String name, String key)
****************************************************
Adds a texture that was loaded with ``gfx.LoadSharedTexture`` to the material that can be used in the shader code.

ShadedMesh:SetParam(String name, float value)
*********************************************
Sets the value of a named parameter in the material.

ShadedMesh:SetParamVec2(String name, float x, float y)
******************************************************
Sets the value of a named 2d vector parameter in the material.

ShadedMesh:SetParamVec3(String name, float x, float y, float z)
***************************************************************
Sets the value of a named 3d vector parameter in the material. Often used for setting RGB

ShadedMesh:SetParamVec4(String name, float x, float y, float z, float w)
************************************************************************
Sets the value of a named 4d vector parameter in the material. Often used for setting RGBA

ShadedMesh:SetData(Vertex[] data)
*************************************
Sets the geometry data for the ShadedMesh object. The format of a vertex is ``{{x,y}, {u,v}}``, see the example
at the beginning of this page for clarification. You can also pass 3D data in the form of ``{{x,y,z}, {u,v}}``, but this is only useful for `ShadedMeshOnTrack` currently.

ShadedMesh:SetBlendMode(int mode)
*********************************
Sets the blending mode for the ShadedMesh object. Available blending modes are::

    BLEND_NORM (default)
    BLEND_ADD
    BLEND_MULT
    
ShadedMesh:SetOpaque(bool opaque)
*********************************
Sets the material as opaque or non-opaque, non-opaque is the default.

ShadedMesh:SetPrimitiveType(int type)
*************************************
Sets the format of the geometry data supplied to the ShadedMesh. Available types are::

    PRIM_TRILIST (default)
    PRIM_TRIFAN
    PRIM_TRISTRIP
    PRIM_LINELIST
    PRIM_LINESTRIP
    PRIM_POINTLIST

ShadedMesh:SetPosition(float x, float y, float z = 0.0f)
********************************************************
Set the current translation of the mesh. For normal `ShadedMesh`, this is relative to the screen. For `ShadedMeshOnTrack`, this is relative to the center of the track crit line.

ShadedMesh:GetPosition()
************************
Returns the current translation as `(x,y,z)`.

ShadedMesh:SetScale(float x, float y, float z = 1.0f)
*****************************************************
Set the scale of the current mesh.

ShadedMesh:GetScale()
*********************
Returns the current scale as `(x,y,z)`.

ShadedMesh:SetRotation(float roll, float yaw = 0.0f, float pitch = 0.0f)
************************************************************************
Sets the rotation of the mesh in degrees. Note: For normal `ShadedMesh`, pitch and yaw may clip, causing part or all of the mesh to be invisible.

ShadedMesh:GetRotation()
************************
Returns the current rotation as `(roll, yaw, pitch)`.

ShadedMesh:SetWireframe(bool useWireframe)
******************************************
Sets the wireframe mode of the object (does not render texture). This can be useful for debugging models or geometry shaders.


ShadedMeshOnTrack
=================
ShadedMeshOnTrack is a ShadedMesh that renders with the track camera instead of the screen.

track.CreateShadedMeshOnTrack(string material = "guiTex")
*********************************************************
Creates a new `ShadedMeshOnTrack` object, the material is loaded from the skin shaders folder where
``material.fs`` and ``material.vs`` need to exist. Note: `track` can only exists in gameplay.lua and in background/foreground scripts.

ShadedMeshOnTrack:UseGameMesh(string meshName)
**********************************************
Uses an existing game mesh (useful for drawing fake buttons with a `ShadedMeshOnTrack`). Current valid meshes are `"button"`,`"fxbutton"`, and `"track"`. The length of the mesh will also be set the correct mesh length.

ShadedMeshOnTrack:SetLength(float length)
*****************************************
Sets the length of the mesh (length in the y direction relative to the track), which is used in `ShadedMeshOnTrack:ScaleToLength`. If you use `ShadedMeshOnTrack:UseGameMesh`, the length will already be set. You can also use these constants::

    BUTTON_TEXTURE_LENGTH
    FXBUTTON_TEXTURE_LENGTH
    TRACK_LENGTH

ShadedMeshOnTrack:GetLength()
*****************************
Return the length of the mesh if previously set.



ShadedMeshOnTrack:ScaleToLength(float length)
*********************************************
This will set the y scale of the mesh based on the mesh length (set with `SetLength` or `UseGameMesh`). This simplifies scaling the mesh to a size relative to the track. You would use this when creating fake notes which may have variable length based on duration.

ShadedMeshOnTrack:SetClipWithTrack(bool doClip)
***********************************************
If clipping is enabled, parts of meshes beyond the end of the track will not render.

ShadedMeshOnTrack

