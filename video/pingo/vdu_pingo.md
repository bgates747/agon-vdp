# 3D Rendering via Pingo on Agon

## Introduction to 3D Render Commands

A 3D scene is composed of one or more object models (meshes), each of which uses
some texture for colorizing the model. A texture uses a bitmap for its
pixel colors, and the model uses texture coordinates to assign colors to its
surfaces when the render is performed.

There are various mathematical computations involved in 3D rendering, of course,
and floating point numbers are used for many of those computations. However, VDU
commands do not directly support passing floating point numbers, as the VDU statement
only supports passing 1-byte and 2-byte values. For that reason, many of the
values passed to the render commands are scaled values.

The commands below use numbers with the following meaning and ranges:
<br><br><b>sid</b>: A VDU buffer ID that acts as a scene ID. It refers to a control structure for the 3D scene.
<br><br><b>mid</b>: A specific mesh ID in the range 0 to 65535.
<br><br><b>oid</b>: A specific object ID in the range 0 to 65535.
<br><br><b>n</b>: A positive number (count) of things that follow within the same command.
<br><br><b>x</b>: A 2D X coordinate in the range -32768 to +32767. Often, the value is in or near the range of 0 to screen width.
<br><br><b>y</b>: A 2D Y coordinate in the range -32768 to +32767. Often, the value is in or near the range of 0 to screen height.
<br><br><b>w</b>: A positive width that typically ranges from 1 to screen width.
<br><br><b>h</b>: A positive height that typically ranges from 1 to screen height.
<br><br><b>x0, y0, z0</b>: A prescaled 3D X, Y, or Z coordinate in the range -32767 to +32767.
This number is divided by 32767 to yield a floating point number in the range -1.0 to +1.0, for 3D computations.
To prescale a set of coordinates for use in a VDU command, scale them all to fit within the range -1.0 to +1.0,
then multiply the new floating point values by 32767.
```
F = FACTOR * 32767
PX = X * F
PY = Y * F
PZ = Z * F
VDU ... PX; PY; PZ; ...
```
<br><br><b>i0</b>: A zero-based index into a list of coordinates (mesh or texture).
<br><br><b>u0, v0</b>: A texture coordinate ranging from 0 to 65535.
This value is divided by 65535 to yield a floating point number, for 3D computations.
To prescale a set of coordinates for use in a VDU command, scale them all to fit within the range 0.0 to +1.0,
then multiply the new floating point values by 65535, as appropriate.
```
PU = U * HORIZ_FACTOR * 65535
PV = V * VERT_FACTOR * 65535
VDU ... PU; PV; ...
```
<br><br><b>scalex, scaley, scalez</b>: A prescaled 3D X, Y, or Z scale value in the range 0 to 65535.
This number is divided by 256 to yield a floating point number in the approximate range 0.0 to near 256.0, for 3D computations.
To prescale a set of scale factors for use in a VDU command, multiply them by 256.
```
F = 256
PSX = SX * F
PSY = SY * F
PSZ = SZ * F
VDU ... PSX; PSY; PSZ; ...
```
<br><br><b>anglex, angley, anglez</b>: A prescaled 3D X, Y, or Z rotation angle value in the range -32767 to +32767.
This number is divided by 32767 to yield a floating point number in the range -1.0 to +1.0, for 3D computations.
The resulting number is multiplied by 2PI, to yield an angle in radians.
Thus, the passed value of -32767 means -2PI, and +32767 means +2PI.
To prescale a set of angles in radians for use in a VDU command, divide the angles by 2PI, which will
scale them all to fit within the range -1.0 to +1.0,
then multiply the new floating point values by 32767.
```
F = 32767 / TWOPI
PAX = AX * F
PAY = AY * F
PAZ = AZ * F
VDU ... PAX; PAY; PAZ; ...
```
<br><br><b>distx, disty, distz</b>: A prescaled 3D X, Y, or Z translation distance in the range -32767 to +32767.
This number is divided by 32767 to yield a floating point number in the range -1.0 to +1.0, for 3D computations.
The resulting number is multiplied by 256.0.
Thus, the passed value of -32767 means -256.0, and +32767 means +256.0.
To prescale a set of distances for use in a VDU command, divide the distances by 256,
scale them all to fit within the range -1.0 to +1.0,
then multiply the new floating point values by 32767.
```
F = 32767 / 256
PDX = DX * F
PDY = DY * F
PDZ = DZ * F
VDU ... PDX; PDY; PDZ; ...
```
<br><br><b>lightx, lighty, lightz</b>: Signed 16-bit components of a
directional-light vector. The components describe a ratio, so they do not need
a particular fixed-point scale. Pingo normalizes every accepted vector. The
all-zero vector is invalid and leaves the current direction unchanged.
<br><br><b>intensity, ambient</b>: Unsigned 8-bit illumination values. A value
of 127 is unity (`1.0`), 0 is zero, and 255 is approximately `2.008` times
unity. Values above unity deliberately overdrive the color channels; each
channel saturates at its maximum rather than wrapping.
<br>

# VDU Commands for 3D Rendering

## Overview of Commands

<b>VDU 23, 0, &A0, sid; &49, 0, w; h;</b> :  Create Control Structure<br>
<b>VDU 23, 0, &A0, sid; &49, 1, mid; n; x0; y0; z0; ...</b> :  Define Mesh Vertices<br>
<b>VDU 23, 0, &A0, sid; &49, 2, mid; n; i0; ...</b> :  Set Mesh Vertex Indexes<br>
<b>VDU 23, 0, &A0, sid; &49, 3, mid; n; u0; v0; ...</b> :  Define Mesh Texture Coordinates<br>
<b>VDU 23, 0, &A0, sid; &49, 4, mid; n; i0; ...</b> :  Set Texture Coordinate Indexes<br>
<b>VDU 23, 0, &A0, sid; &49, 5, oid; mid; bmid;</b> :  Create Object<br>
<b>VDU 23, 0, &A0, sid; &49, 40, oid; n; u0; v0; ...</b> :  Define Object Texture Coordinates<br>
<b>VDU 23, 0, &A0, sid; &49, 6, oid; scalex;</b> :  Set Object X Scale Factor<br>
<b>VDU 23, 0, &A0, sid; &49, 7, oid; scaley;</b> :  Set Object Y Scale Factor<br>
<b>VDU 23, 0, &A0, sid; &49, 8, oid; scalez;</b> :  Set Object Z Scale Factor<br>
<b>VDU 23, 0, &A0, sid; &49, 9, oid; scalex; scaley; scalez;</b> :  Set Object XYZ Scale Factors<br>
<b>VDU 23, 0, &A0, sid; &49, 10, oid; anglex;</b> :  Set Object X Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 11, oid; angley;</b> :  Set Object Y Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 12, oid; anglez;</b> :  Set Object Z Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 13, oid; anglex; angley; anglez;</b> :  Set Object XYZ Rotation Angles<br>
<b>VDU 23, 0, &A0, sid; &49, 14, oid; distx;</b> :  Set Object X Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 15, oid; disty;</b> :  Set Object Y Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 16, oid; distz;</b> :  Set Object Z Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 17, oid; distx; disty; distz;</b> :  Set Object XYZ Translation Distances<br>
<b>VDU 23, 0, &A0, sid; &49, 18, anglex;</b> :  Set Camera X Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 19, angley;</b> :  Set Camera Y Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 20, anglez;</b> :  Set Camera Z Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 21, anglex; angley; anglez;</b> :  Set Camera XYZ Rotation Angles<br>
<b>VDU 23, 0, &A0, sid; &49, 22, distx;</b> :  Set Camera X Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 23, disty;</b> :  Set Camera Y Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 24, distz;</b> :  Set Camera Z Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 25, distx; disty; distz;</b> :  Set Camera XYZ Translation Distances<br>
<b>VDU 23, 0, &A0, sid; &49, 26, scalex;</b> :  Set Scene X Scale Factor<br>
<b>VDU 23, 0, &A0, sid; &49, 27, scaley;</b> :  Set Scene Y Scale Factor<br>
<b>VDU 23, 0, &A0, sid; &49, 28, scalez;</b> :  Set Scene Z Scale Factor<br>
<b>VDU 23, 0, &A0, sid; &49, 29, scalex; scaley; scalez;</b> :  Set Scene XYZ Scale Factors<br>
<b>VDU 23, 0, &A0, sid; &49, 30, anglex;</b> :  Set Scene X Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 31, angley;</b> :  Set Scene Y Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 32, anglez;</b> :  Set Scene Z Rotation Angle<br>
<b>VDU 23, 0, &A0, sid; &49, 33, anglex; angley; anglez;</b> :  Set Scene XYZ Rotation Angles<br>
<b>VDU 23, 0, &A0, sid; &49, 34, distx;</b> :  Set Scene X Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 35, disty;</b> :  Set Scene Y Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 36, distz;</b> :  Set Scene Z Translation Distance<br>
<b>VDU 23, 0, &A0, sid; &49, 37, distx; disty; distz;</b> :  Set Scene XYZ Translation Distances<br>
<b>VDU 23, 0, &A0, sid; &49, 38, bmid;</b> :  Render To Bitmap<br>
<b>VDU 23, 0, &A0, sid; &49, 39</b> :  Delete Control Structure<br>
<b>VDU 23, 0, &A0, sid; &49, 41, mode, token;</b> :  Configure Render-Completion Notification<br>
<b>Subcommand 42 is reserved and must not be used.</b><br>
<b>VDU 23, 0, &A0, sid; &49, 43, lightx; lighty; lightz;</b> :  Set Light Direction<br>
<b>VDU 23, 0, &A0, sid; &49, 44, intensity</b> :  Set Light Intensity<br>
<b>VDU 23, 0, &A0, sid; &49, 45, ambient</b> :  Set Ambient-Light Floor<br>
<b>VDU 23, 0, &A0, sid; &49, 46, enabled</b> :  Enable or Disable Illumination<br>
<b>VDU 23, 0, &A0, sid; &49, 47, mid; mode</b> :  Set Mesh Shading Mode<br>
<b>VDU 23, 0, &A0, sid; &49, 48, mid; mode</b> :  Set Mesh Illumination Policy<br>

## Create Control Structure
<b>VDU 23, 0, &A0, sid; &49, 0, w; h;</b> :  Create Control Structure<br>

This command initializes a control structure used to
do 3D rendering. The structure is housed inside the designated buffer. The buffer
referred to by the scene ID (sid) is created, if it does not already exist.

The given width and height determine the size of the final rendered scene.

## Define Mesh Vertices
<b>VDU 23, 0, &A0, sid; &49, 1, mid; n; x0; y0; z0; ...</b> :  Define Mesh Vertices

This command establishes the list of mesh coordinates to be used to define
a surface structure. The mesh may be referenced by multiple objects.

The "n" parameter is the number of vertices, so the total number of coordinates specified equals n*3.

## Set Mesh Vertex Indexes
<b>VDU 23, 0, &A0, sid; &49, 2, mid; n; i0; ...</b> :  Set Mesh Vertex Indexes

This command lists the indexes of the vertices that define a 3D mesh. Individual
vertices are often referenced multiple times within a mesh, because they are
often part of multiple surface triangles. Each index value ranges from 0 to
the number of defined mesh vertices.

The "n" parameter is the number of indexes, and must match the "n" in subcommand 4
(Set Texture Coordinate Indexes).

## Define Mesh Texture Coordinates
<b>VDU 23, 0, &A0, sid; &49, 3, mid; n; u0; v0; ...</b> :  Define Mesh Texture Coordinates

This command establishes the list of U/V texture coordinates that define texturing
for a mesh. For any object that does not have its texture coordinates set separately,
those texture coordinates belonging to the mesh that is referenced by the object
will be employed. See also subcommand #40.

The mesh must still have texture indexes established.

The "n" parameter is the number of coordinate pairs, so the total number of coordinates specified equals n*2.

## Set Texture Coordinate Indexes
<b>VDU 23, 0, &A0, sid; &49, 4, mid; n; i0; ...</b> :  Set Texture Coordinate Indexes

This command lists the indexes of the coordinates that define a 3D texture for a mesh.
Individual coordinates may be referenced multiple times within a texture,
but that is not required. The number of indexes passed in this command must match
the number of mesh indexes defining the mesh. Thus, each mesh vertex has texture
coordinates associated with it.

The "n" parameter is the number of indexes, and must match the "n" in subcommand 2
(Set Mesh Vertex Indexes).

## Define Object
<b>VDU 23, 0, &A0, sid; &49, 5, oid; mid; bmid;</b> :  Create Object

This command defines a renderable object in terms of its already-defined mesh,
plus a reference to an existing bitmap that provides its coloring, via the
texture coordinates used by the mesh. The same mesh can be used multiple times,
with the same or different bitmaps for coloring. The bitmap must be in the
RGBA2222 (1 byte per pixel) or RGBA8888 (4 bytes per pixel) format. Pingo's
native working format is RGBA2222; RGBA8888 remains supported for compatibility.

## Define Object Texture Coordinates
<b>VDU 23, 0, &A0, sid; &49, 40, oid; n; u0; v0; ...</b> :  Define Object Texture Coordinates

This command establishes the list of U/V texture coordinates that define texturing
for an object, as opposed to a mesh. For that object, it will override any texture coordinates defined
for the mesh that the object references. See also subcommand #3.

The mesh must still have texture indexes established.

The "n" parameter is the number of coordinate pairs, so the total number of coordinates specified equals n*2.

## Set Object X Scale Factor
<b>VDU 23, 0, &A0, sid; &49, 6, oid; scalex;</b> :  Set Object X Scale Factor

This command sets the X scale factor for an object.

## Set Object Y Scale Factor
<b>VDU 23, 0, &A0, sid; &49, 7, oid; scaley;</b> :  Set Object Y Scale Factor

This command sets the Y scale factor for an object.

## Set Object Z Scale Factor
<b>VDU 23, 0, &A0, sid; &49, 8, oid; scalez;</b> :  Set Object Z Scale Factor

This command sets the Z scale factor for an object.

## Set Object XYZ Scale Factors
<b>VDU 23, 0, &A0, sid; &49, 9, oid; scalex; scaley; scalez;</b> :  Set Object XYZ Scale Factors

This command sets the X, Y, and Z scale factors for an object.

## Set Object X Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 10, oid; anglex;</b> :  Set Object X Rotation Angle

This command sets the X rotation angle for an object.

## Set Object Y Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 11, oid; angley;</b> :  Set Object Y Rotation Angle

This command sets the Y rotation angle for an object.

## Set Object Z Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 12, oid; anglez;</b> :  Set Object Z Rotation Angle

This command sets the Z rotation angle for an object.

## Set Object XYZ Rotation Angles
<b>VDU 23, 0, &A0, sid; &49, 13, oid; anglex; angley; anglez;</b> :  Set Object XYZ Rotation Angles

This command sets the X, Y, and Z rotation angles for an object.

## Set Object X Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 14, oid; distx;</b> :  Set Object X Translation Distance

This command sets the X translation distance for an object.
Note that 3D translation of an object is independent of 2D translation of the the rendered bitmap.

## Set Object Y Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 15, oid; disty;</b> :  Set Object Y Translation Distance

This command sets the Y translation distance for an object.
Note that 3D translation of an object is independent of 2D translation of the the rendered bitmap.

## Set Object Z Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 16, oid; distz;</b> :  Set Object Z Translation Distance

This command sets the Z translation distance for an object.
Note that 3D translation of an object is independent of 2D translation of the the rendered bitmap.

## Set Object XYZ Translation Distances
<b>VDU 23, 0, &A0, sid; &49, 17, oid; distx; disty; distz;</b> :  Set Object XYZ Translation Distances

This command sets the X, Y, and Z translation distances for an object.
Note that 3D translation of an object is independent of 2D translation of the the rendered bitmap.

## Set Camera X Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 18, anglex;</b> :  Set Camera X Rotation Angle

This command sets the X rotation angle for the camera.

## Set Camera Y Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 19, angley;</b> :  Set Camera Y Rotation Angle

This command sets the Y rotation angle for the camera.

## Set Camera Z Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 20, anglez;</b> :  Set Camera Z Rotation Angle

This command sets the Z rotation angle for the camera.

## Set Camera XYZ Rotation Angles
<b>VDU 23, 0, &A0, sid; &49, 21, anglex; angley; anglez;</b> :  Set Camera XYZ Rotation Angles

This command sets the X, Y, and Z rotation angles for the camera.

## Set Camera X Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 22, distx;</b> :  Set Camera X Translation Distance

This command sets the X translation distance for the camera.
Note that 3D translation of the camera is independent of 2D translation of the the rendered bitmap.

## Set Camera Y Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 23, disty;</b> :  Set Camera Y Translation Distance

This command sets the Y translation distance for the camera.
Note that 3D translation of the camera is independent of 2D translation of the the rendered bitmap.

## Set Camera Z Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 24, distz;</b> :  Set Camera Z Translation Distance

This command sets the Z translation distance for the camera.
Note that 3D translation of the camera is independent of 2D translation of the the rendered bitmap.

## Set Camera XYZ Translation Distances
<b>VDU 23, 0, &A0, sid; &49, 25, distx; disty; distz;</b> :  Set Camera XYZ Translation Distances

This command sets the X, Y, and Z translation distances for the camera.
Note that 3D translation of the camera is independent of 2D translation of the the rendered bitmap.

## Set Scene X Scale Factor
<b>VDU 23, 0, &A0, sid; &49, 26, scalex;</b> :  Set Scene X Scale Factor

This command sets the X scale factor for the scene.

## Set Scene Y Scale Factor
<b>VDU 23, 0, &A0, sid; &49, 27, scaley;</b> :  Set Scene Y Scale Factor

This command sets the Y scale factor for the scene.

## Set Scene Z Scale Factor
<b>VDU 23, 0, &A0, sid; &49, 28, scalez;</b> :  Set Scene Z Scale Factor

This command sets the Z scale factor for the scene.

## Set Scene XYZ Scale Factors
<b>VDU 23, 0, &A0, sid; &49, 29, scalex; scaley; scalez;</b> :  Set Scene XYZ Scale Factors

This command sets the X, Y, and Z scale factors for the scene.

## Set Scene X Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 30, anglex;</b> :  Set Scene X Rotation Angle

This command sets the X rotation angle for the scene.

## Set Scene Y Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 31, angley;</b> :  Set Scene Y Rotation Angle

This command sets the Y rotation angle for the scene.

## Set Scene Z Rotation Angle
<b>VDU 23, 0, &A0, sid; &49, 32, anglez;</b> :  Set Scene Z Rotation Angle

This command sets the Z rotation angle for the scene.

## Set Scene XYZ Rotation Angles
<b>VDU 23, 0, &A0, sid; &49, 33, anglex; angley; anglez;</b> :  Set Scene XYZ Rotation Angles

This command sets the X, Y, and Z rotation angles for the scene.

## Set Scene X Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 34, distx;</b> :  Set Scene X Translation Distance

This command sets the X translation distance for the scene.
Note that 3D translation of the scene is independent of 2D translation of the the rendered bitmap.

## Set Scene Y Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 35, disty;</b> :  Set Scene Y Translation Distance

This command sets the Y translation distance for the scene.
Note that 3D translation of the scene is independent of 2D translation of the the rendered bitmap.

## Set Scene Z Translation Distance
<b>VDU 23, 0, &A0, sid; &49, 36, distz;</b> :  Set Scene Z Translation Distance

This command sets the Z translation distance for the scene.
Note that 3D translation of the scene is independent of 2D translation of the the rendered bitmap.

## Set Scene XYZ Translation Distances
<b>VDU 23, 0, &A0, sid; &49, 37, distx; disty; distz;</b> :  Set Scene XYZ Translation Distances

This command sets the X, Y, and Z translation distances for the scene.
Note that 3D translation of the scene is independent of 2D translation of the the rendered bitmap.

## Render To Bitmap
<b>VDU 23, 0, &A0, sid; &49, 38, bmid;</b> :  Render To Bitmap

This command uses information provided by the above commands to render the 3D scene
onto the specified bitmap. This command must be used in
order to perform the render operation; it does <i>not</i> happen automatically, when other
commands change some of the render parameters.

The destination bitmap may use RGBA2222 or RGBA8888. Pingo renders directly to
an RGBA2222 destination. An RGBA8888 destination uses Pingo's private RGBA2222
frame and is expanded to RGBA8888 before the render-completion notification is
sent.

## Delete Control Structure
<b>VDU 23, 0, &A0, sid; &49, 39</b> :  Delete Control Structure<br>

This command deinitializes an existing control structure,
assuming that it exists in the designated buffer. The buffer is subsequently
deleted through the canonical buffered-VDU clear path, releasing the scene's
owned Pingo resources and buffer metadata.

## Configure Render-Completion Notification
<b>VDU 23, 0, &A0, sid; &49, 41, mode, token;</b> :  Configure Render-Completion Notification

This command opts a scene into or out of a render-completion callback. Mode 0
disables notification. Mode 1 emits a stock MOS keyboard-event packet after a
successful render has been completely written to its destination. Other mode
values are treated as disabled. The caller-supplied 16-bit token is returned in
the event, allowing an application to distinguish its own completion messages.
Notification is disabled by default.

The ten-byte event payload is:

```
'P', '3', 'D', 'R', version, event, token_lo, token_hi, sequence_lo, sequence_hi
```

The current protocol uses `version = 1` and `event = 1` for render completion.
`sequence` is the scene's low 16 bits of its monotonically increasing render
sequence number.

## Reserved Subcommand 42

Subcommand 42 is reserved for compatibility with historical Pingo branches,
where it represented `camera_track_object(oid;)`. It is not implemented by this
firmware and must not be reused or issued. In particular, sending the historical
payload to current firmware would leave those bytes in the VDU stream.

## Set Light Direction
<b>VDU 23, 0, &A0, sid; &49, 43, lightx; lighty; lightz;</b> :  Set Light Direction

This command sets the scene-wide directional-light vector. Each component is a
signed little-endian 16-bit integer. Pingo treats the three values as a ratio
and normalizes the vector once when the command is received. The all-zero vector
is rejected without changing the existing direction.

The default direction is normalized `(0, +1, -1)`, placing the light above and
on the negative-Z side of the scene.

## Set Light Intensity
<b>VDU 23, 0, &A0, sid; &49, 44, intensity</b> :  Set Light Intensity

This command sets the unsigned 8-bit multiplier applied to the directional
illumination. The multiplier is `intensity / 127`: 0 is dark, 127 is unity, and
255 is approximately 2.008. Overdriven color channels saturate at their maximum.
The default is 127.

## Set Ambient-Light Floor
<b>VDU 23, 0, &A0, sid; &49, 45, ambient</b> :  Set Ambient-Light Floor

This command sets the scene-wide minimum illumination using the same
`value / 127` scale as intensity. Ambient light is a floor, not an additive
term: a face receives at least this much illumination even when its directional
term is smaller. The default is 0.

With illumination enabled, the per-face shade multiplier is:

```
directional = clamp((1 + dot(face_normal, light_direction)) / 2, 0, 1)
shade       = max(ambient / 127, directional * intensity / 127)
```

## Enable or Disable Illumination
<b>VDU 23, 0, &A0, sid; &49, 46, enabled</b> :  Enable or Disable Illumination

An `enabled` value of 1 enables scene lighting; 0 disables it. Other values are
invalid and leave the prior state unchanged. With illumination disabled, Pingo
skips the normal, dot-product, and shade-table work and writes native texture or
flat-palette colors. Illumination is enabled by default.

Illumination and mesh shading mode are independent. Flat-palette triangles are
illuminated normally when illumination is enabled and retain their native
palette color when it is disabled.

The `esp32dev-pingo-unlit` PlatformIO environment defines
`PINGO_DISABLE_ILLUMINATION=1`. That diagnostic build removes illumination at
compile time, so runtime command 46 cannot turn it back on.

## Set Mesh Shading Mode
<b>VDU 23, 0, &A0, sid; &49, 47, mid; mode</b> :  Set Mesh Shading Mode

This command selects the shading mode for a mesh. The 16-bit mesh ID is followed
by an unsigned byte:

- Mode 0: perspective-correct textured rendering.
- Mode 1: flat-palette rendering, with one constant sampled color per source
  triangle.

Mode 0 is the default. Invalid modes are rejected without changing or creating
the mesh.

Flat-palette mode samples the first UV of each original source triangle once.
All triangles generated from that source by frustum clipping retain the same
sampled color. Geometry clipping, depth testing, span ownership, RGBA2222 output,
and optional illumination continue through the established Pingo renderer;
RGBA8888 destinations still receive the normal compatibility expansion.

Asset-build tooling must ensure that all three UVs of a flat-shaded source
triangle select the same cell in the reference palette; malformed multi-color
triangles should be rejected before upload. The firmware deliberately does not
reinterpret or rewrite the UV data.

## Set Mesh Illumination Policy
<b>VDU 23, 0, &A0, sid; &49, 48, mid; mode</b> :  Set Mesh Illumination Policy

This command selects how every object using a mesh responds to the scene-wide
illumination state. The 16-bit mesh ID is followed by an unsigned byte:

- Mode 0: inherit scene illumination.
- Mode 1: self-illuminated; emit native texture or flat-palette colors.

Mode 0 is the default and preserves the behavior of existing applications.
Mode 1 bypasses the face-normal, directional-light, ambient-floor, and
shade-table work for that mesh. It does not bypass geometry transforms,
clipping, depth testing, texture mapping, or flat-palette selection. Shading
mode and illumination policy are independent: either a textured or a
flat-palette mesh may be scene-lit or self-illuminated.

A valid policy may be selected before mesh geometry is uploaded, and later
component uploads retain it. Invalid modes are rejected without changing or
creating the mesh. The policy is mesh-owned, so it applies to every object that
references that mesh.

When scene illumination is disabled with subcommand 46, inherited meshes also
emit native colors. The `PINGO_DISABLE_ILLUMINATION=1` diagnostic build retains
its compile-time behavior: all meshes emit native colors regardless of their
runtime illumination policy.

## Sample

The following image illustrates the concept.

![Render](render.png)
