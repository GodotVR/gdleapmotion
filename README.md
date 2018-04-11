# Godot Leap Motion module

This module implements an interface for the Leap motion sensor

Compiling
---------

I've only tested compiling on Windows using Visual Studio 2017.
You need to have Python 2.7 and Scons installed

You also need to download the leap motion SDK here:
https://developer.leapmotion.com/
We are using the LeapC API that was added to the new Orion Beta.

Make sure submodules have been downloaded.
You also need a copy of Godot 3.0.3 (out soon) or newer, the instructions below assume you have copied the godot.exe into the folder containing this readme.

These instructions also assume you are compiling on a 64bit environment.

Compile the cpp bindings:
```
cd godot-cpp
scons platform=windows godotbinpath=..\godot.exe generate_bindings=yes
cd ..
```

Compile our module:
```
scons platform=windows leapsdk_path=<path_to_leap_motion_sdk>
```

(for convenience I'm including my latest 64bit compile but I can't guarantee it will consistantly work)
