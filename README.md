# Viewport-Editor-Gameplay3D
Viewport editor made with Autodesk Mayas C++ API and GamePlay3D [Open-Source C++ game engine]. 

## Plugin
This plugin uses shared memory with a circular buffer to send the information from Autodesk Mayas viewport into the GamePlay3D Engine. In this project the plugin is created to take informations from the C++ APIs callbacks such as:
- Created meshes 
- Deleted meshes
- Translation/Rotation/Scale on meshes
- Materials [New added and updated materials on a mesh]
- Textures [Texture information on materials]
- Camera information [If you change camera in viewport, zoom in/out]
- Changed topology [Moving vertices/ bevel, smooth, extrude, "adding divitions"]
