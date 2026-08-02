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
<b>VDU 23, 0, &A0, sid; &49, 49, pattern_buffer; lookup_buffer; pattern_count; material_count, band_count</b> :  Bind Flat-Pattern Library (Experimental)<br>
<b>VDU 23, 0, &A0, sid; &49, 50, stage_bmid; mid; V; I; U; T;</b> :  Atomically Replace Mesh from Consolidated Buffer (Experimental)<br>
<b>VDU 23, 0, &A0, sid; &49, 51, oid; active</b> :  Set Object Active State (Experimental)<br>
<b>VDU 23, 0, &A0, sid; &49, 52, oid; distx24; disty24; distz24</b> :  Set Object XYZ Wide Translation Distances (Experimental)<br>
<b>VDU 23, 0, &A0, sid; &49, 53, far_units;</b> :  Set Projection Far Distance (Experimental)<br>

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
skips the normal, dot-product, and shade-table work. Textured and flat-palette
meshes write native colors; flat-pattern meshes select the library's final
illumination band. Illumination is enabled by default.

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
- Mode 2: experimental flat-pattern rendering, with one precomputed 4x4 native
  RGBA2222 pattern selected per source triangle.

Mode 0 is the default. Invalid modes are rejected without changing or creating
the mesh.

Flat-palette mode samples the first UV of each original source triangle once.
Flat-pattern mode instead interprets that UV's clamped, row-major texel
position as a material ID; the texel color is irrelevant. All triangles
generated from that source by frustum clipping retain the same sampled color or
selected pattern. Geometry clipping, depth testing, span ownership, RGBA2222
output, and optional illumination continue through the established Pingo
renderer; RGBA8888 destinations still receive the normal compatibility
expansion.

Asset-build tooling must ensure that all three UVs of a flat-shaded source
triangle select the same cell in the reference palette; malformed multi-color
triangles should be rejected before upload. The firmware deliberately does not
reinterpret or rewrite the UV data.

## Bind Flat-Pattern Library (Experimental)
<b>VDU 23, 0, &A0, sid; &49, 49, pattern_buffer; lookup_buffer; pattern_count; material_count, band_count</b> :  Bind Flat-Pattern Library<br>

This command installs the resources used by mesh shading mode 2. The fixed
eight-byte payload is:

```
pattern_buffer:u16
lookup_buffer:u16
pattern_count:u16
material_count:u8
band_count:u8
```

The pattern buffer must contain exactly `pattern_count * 16` bytes in one
consolidated block. Each consecutive 16-byte record is a row-major 4x4 pattern
of opaque native RGBA2222 pixels. The lookup buffer must contain exactly
`material_count * band_count` bytes in one consolidated block. Lookup entries
are zero-based pattern IDs in material-major order:

```
lookup[material_id * band_count + illumination_band]
```

Accepted ranges are 1 through 256 patterns, 1 through 255 materials, and 2
through 255 illumination bands. Every pattern pixel must have RGBA2222 alpha 3,
and every lookup entry must be less than `pattern_count`. Buffer 65535, a live
Pingo control buffer, identical source IDs, mixed-zero fields, wrong sizes, and
multi-block resources are rejected.

Pingo validates the complete candidate and takes a private immutable snapshot
of both buffers. Clearing, replacing, or modifying the generic source buffers
after a successful bind therefore does not change an active scene. Malformed,
truncated, or allocation-failed commands preserve the previous binding. A
payload in which all five fields are zero explicitly removes it.

The application normally supplies a compact selector bitmap and assigns each
source triangle a UV whose first coordinate selects material texel position 0
through `material_count - 1`. A 4x4 bitmap therefore carries up to 16 material
IDs even though its pixel values are unused. Invalid material IDs, absent
libraries, and invalid retained pattern IDs reject the affected object or face
before any depth-buffer write.

For a scene-lit face, Pingo chooses the nearest endpoint-inclusive band:

```
illumination_band = round(clamp(shade, 0, 1) * (band_count - 1))
```

Self-illuminated meshes, globally unlit scenes, and the compile-time unlit build
select the final band. The chosen native pattern already encodes the intended
illumination and is written without a second shade operation. Its phase is
global screen space, `pattern[(y & 3) * 4 + (x & 3)]`, so adjacent triangles
and independently culled terrain chunks do not restart or expose pattern seams.

This command and mode 2 are experimental while the terrain fixture qualifies
their visual quality and performance. Modes 0 and 1 are unchanged and do not
depend on a flat-pattern library.

## Atomically Replace Mesh from Consolidated Buffer (Experimental)
<b>VDU 23, 0, &A0, sid; &49, 50, stage_bmid; mid; V; I; U; T;</b> :  Atomically Replace Mesh from Consolidated Buffer<br>

This fixed twelve-byte command replaces all four array components of an
already-established mesh slot. Its little-endian payload is:

```
stage_bmid:u16
mid:u16
V:u16
I:u16
U:u16
T:u16
```

`stage_bmid` must identify an ordinary generic buffer containing exactly one
consolidated block. A live Pingo control, buffer 0, buffer 65535, a missing
buffer, or a multi-block buffer is invalid. `mid` must already exist; an
application may establish an empty stable slot with subcommand 47 or 48 before
its first import. Requiring that slot avoids a map-node allocation at commit
time and preserves the address held by every object already bound to it.

The staging block has no header and contains four consecutive packed arrays:

```
positions:        3 * V signed s16 values (x, y, z)
position_indices: I     u16 values
texture_coords:   2 * U u16 values (u, v)
texture_indices:  T     u16 values
```

Every value is little-endian. The exact staging size is therefore:

```
6 * V + 2 * I + 4 * U + 2 * T bytes
```

Positions use the established Pingo vertex conversion, signed value divided by
32767; consequently -32768 remains slightly below -1 exactly as with
subcommand 1. Texture coordinates use the established unsigned value divided
by 65535. Accepted counts are `V >= 3`, `I >= 3`, `I % 3 == 0`, `U >= 1`, and
`T == I`. Every position index must be less than `V`, every texture index must
be less than `U`, and all converted coordinates must be finite.

When the target slot is already in flat-palette or flat-pattern shading mode,
all three indexed UV pairs of each source face must be identical. This gives
each flat face one unambiguous selector. Textured slots retain independent UVs
at their three corners.

Pingo allocates and fills four private native arrays, computes geometry
validity and model-space bounds, and validates the complete candidate before
publishing any part of it. It then replaces the four arrays in the existing
map-resident mesh and refreshes every dependent object's texture-mapping
validity. The mesh's shading mode and illumination policy are retained.
Objects bound to the slot keep the same mesh pointer.

Missing, malformed, truncated, out-of-range, inconsistent, or
allocation-failed commands preserve the prior mesh and its object bindings.
The accepted arrays do not borrow the staging storage, so subsequent generic
buffer edits cannot change them. Pingo deliberately does not clear
`stage_bmid`; the caller may reuse or clear it after the command.

Diagnostic builds emit one machine-readable `PINGO_STREAM mesh_replace=ok`
line after atomic publication. It includes the staging and mesh IDs, exact byte
and element counts, and firmware-side conversion/publication time in
microseconds. A rejected command emits no success line; its existing
reason-specific debug message identifies the validation or allocation failure.

## Set Object Active State (Experimental)
<b>VDU 23, 0, &A0, sid; &49, 51, oid; active</b> :  Set Object Active State<br>

The 16-bit object ID is followed by one byte. `active=1` enables rendering and
`active=0` disables it. Objects are active by default, including objects
created by older applications. The object must already exist, and any other
value is invalid; invalid, absent-object, and truncated commands preserve
state and do not create a placeholder object.

An inactive VDU object is not added to the transient render scene, so it does
not consume one of that scene's renderable slots. The native object renderer
also rejects an inactive object at entry, before geometry, material, frustum,
triangle, depth, or diagnostic work. Its transforms, texture binding, mesh
binding, and mesh contents remain intact, so reactivation is a constant-size
control operation. This command is intended to hide a streaming slot while its
next mesh is prepared or when its terrain tile is outside the application's
working set.

Diagnostic builds emit `PINGO_STREAM object_active=ok` with the object ID and
new state. They also emit `PINGO_SCENE active_overflow=1` if an application
nevertheless exceeds the fixed 32-renderable scene budget; objects at and after
the reported map-ordered ID are omitted from that frame.

## Set Object XYZ Wide Translation Distances (Experimental)
<b>VDU 23, 0, &A0, sid; &49, 52, oid; distx24; disty24; distz24</b> :  Set Object XYZ Wide Translation Distances<br>

This command sets all three translation components of an already-established
object using an eleven-byte little-endian payload:

```
oid:u16
distx24:s24
disty24:s24
distz24:s24
```

Each distance is a signed two's-complement 24-bit integer in the same raw unit
used by the legacy 16-bit object translation commands 14 through 17. The
native conversion remains:

```
translation = signed_raw * (256 / 32767)
```

Consequently, sign-extending a command-17 value into 24 bits produces the same
native transform. At the terrain project's accepted 1:8 scale, applications
normally treat about 16 raw counts as one metre. The full range
`-8388608..+8388607` then spans approximately -524,288 through +524,288 metres.

The object must already exist. Pingo always reads the complete fixed payload
before checking that stable object slot, so a valid packet naming an absent
object cannot disrupt the following VDU command and does not create a
placeholder. A truncated packet or absent object preserves the previous scene.
All three components commit together and mark the object transform dirty.

This is deliberately object-only. A floating-origin application keeps its
camera local and continues to use command 25; no wide camera or scene command
is defined without a demonstrated caller. This command also leaves the
control's retained projection distance unchanged; command 53 owns that
independent setting, whose initialization default is 2,500 units.

Diagnostic builds emit `PINGO_WIDE object_translation=ok` with the object ID
and the three signed raw values after a successful atomic update.

## Set Projection Far Distance (Experimental)
<b>VDU 23, 0, &A0, sid; &49, 53, far_units;</b> :  Set Projection Far Distance<br>

This command sets one control's perspective far plane to an unsigned 16-bit
distance in Pingo world units. Every new control defaults to the legacy
configured value of 2,500 units. The near plane remains fixed at one unit.

This experimental candidate also corrects the perspective matrix that applies
that setting. The earlier near-one coefficients made homogeneous `Z + W`
identically one, so the nominal far plane could never reject finite geometry.
The corrected mapping sends view-space `z=-near` to `Z=0` and `z=-far` to
`Z=-W`, matching Pingo's documented `-W <= Z <= 0` clip volume. Consequently,
applications that never send command 53 now genuinely lose geometry beyond
2,500 units; this is a deliberate renderer correctness change, not bit-exact
legacy output. Perspective scale also changes by about 0.04 percent at that
default distance. Existing scenes therefore require the same emulator and
hardware visual qualification as command-53 users.

Accepted values are 2 through 65,535 inclusive. Zero, one, and a truncated
word are rejected without changing the retained distance. The accepted value
is used the next time that control renders; it does not affect any other Pingo
control. Deleting and recreating a control restores the 2,500-unit default.

At the terrain project's accepted eight-metres-per-Pingo-unit scale, the
legacy default reaches 20 km, 8,000 reaches 64 km, and the protocol maximum is
approximately 524 km. A larger far/near ratio reduces perspective depth
separation, so applications should select the shortest distance that contains
the scenery they actually intend to render. Pingo currently uses a 32-bit
depth buffer, but a changed distance still requires application-level visual
qualification.

Diagnostic builds emit `PINGO_PROJECTION far_set=ok units=...` for each
accepted update. The `PINGO_RENDER` record also reports the applied value as
`far=...`.

## Set Mesh Illumination Policy
<b>VDU 23, 0, &A0, sid; &49, 48, mid; mode</b> :  Set Mesh Illumination Policy

This command selects how every object using a mesh responds to the scene-wide
illumination state. The 16-bit mesh ID is followed by an unsigned byte:

- Mode 0: inherit scene illumination.
- Mode 1: self-illuminated; emit native texture or flat-palette colors, or use
  the final illumination band for flat-pattern meshes.

Mode 0 is the default and preserves the behavior of existing applications.
Mode 1 bypasses the face-normal, directional-light, ambient-floor, and
shade-table work for that mesh. It does not bypass geometry transforms,
clipping, depth testing, texture mapping, flat-palette selection, or
flat-pattern selection. Shading mode and illumination policy are independent:
textured, flat-palette, and flat-pattern meshes may be scene-lit or
self-illuminated.

A valid policy may be selected before mesh geometry is uploaded, and later
component uploads retain it. Invalid modes are rejected without changing or
creating the mesh. The policy is mesh-owned, so it applies to every object that
references that mesh.

When scene illumination is disabled with subcommand 46, inherited meshes also
emit native colors, while flat-pattern meshes select their final band. The
`PINGO_DISABLE_ILLUMINATION=1` diagnostic build retains its compile-time
behavior: textured and flat-palette meshes emit native colors and flat-pattern
meshes select their final band, regardless of runtime illumination policy.

## Sample

The following image illustrates the concept.

![Render](render.png)
