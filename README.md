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

Using Leap Motion in Godot
==========================
There is a small demo application in the demo folder that shows the basics of how the Leap Motion works with Godot as a desktop solution.

The subfolder ```addons``` contains the leap motion driver itself and is also the destination for the compile script. This folder will also be available in the Godot asset library in the near future.
Besides the leap motion driver there are also several support scenes.

The leap motion driver has been implemented as a GDNative driver, in order to use it you need to place a few lines of code in the ```_ready``` function of a script preferably placed on your root node:

```
func _ready():
	# Create a new leap motion resource
	var leap_motion = preload("res://addons/gdleapmotion/gdlm_sensor.gdns").new()
	
	# Tell it what scenes to load when a new hand is detected
	leap_motion.set_left_hand_scene(preload("res://addons/gdleapmotion/scenes/left_hand_with_collisions.tscn"))
	leap_motion.set_right_hand_scene(preload("res://addons/gdleapmotion/scenes/right_hand_with_collisions.tscn"))
	
	# add this as a child node to our scene
	add_child(leap_motion)
```

The first line loads the ```gdlm_sensor.gdns``` file and creates a new node using that script.

The next two lines load scenes that will be instantiated when leap motion detects a new hand. You can create your own if you want the hands to look differently or add additional logic to them. Note that here versions with colliders are loaded.

Finally the last line adds the leap motion node as a child node to your current scene. It is important that the location of this node within your scene becomes the anchor point for the leap motion. Basically this point maps the physical location of your leap motion device to your virtual world.

Using Leap Motion in Godot with a VR headset
============================================
There isn't an example for this in this repository. I may add one later or create a separate demo project for this but support for this has been added in the latest build. This does require Godot 3.0.3 or newer to run as this version of Godot has support for frame timing.

Using the Leap Motion in VR takes an approach that is not immediately apparant. It would seem logical to instantiate the ```gdlm_sensor.gdns``` node as a child node of the headset. While this will work you'll notice placement issues with the hands when you move your head.

Instead this node should be created as a child node of your ARVROrigin node and a special VR mode needs to be turned on. The driver will interact with the ARVR system to obtain the location and orientation of the HMD and perform the needed adjustments for hand placement as the sensor data on the leap motion is independently handled from the HMD.

In this case you would amend the ```_ready``` function to be something like:
```
func _ready():
	# enable vr interface for Oculus
	var interface = ARVRServer.find_interface("Oculus")
	if interface and interface.initialize():
		get_viewport().arvr = true
	
	# Create a new leap motion resource
	leap_motion = preload("res://addons/gdleapmotion/gdlm_sensor.gdns").new()

	# turn VR mode on
	leap_motion.set_arvr(true)
		
	# Tell it what scenes to load when a new hand is detected
	leap_motion.set_left_hand_scene(preload("res://addons/gdleapmotion/scenes/left_hand_with_collisions.tscn"))
	leap_motion.set_right_hand_scene(preload("res://addons/gdleapmotion/scenes/right_hand_with_collisions.tscn"))
	
	# Add it as a child to our origin. Our driver will pair it up with HMD automatically
	get_node("ARVROrigin").add_child(leap_motion)

```

I've added the initialisation of the VR interface here purely for example, the new bits are calling ```set_arvr``` and adding the node as a child node of our ARVROrigin node.

There are two more functions available you can add into the logic above:
```
	# you can change the smoothing we apply to the positioning though it seems to be ok even without. 1.0 is no smoothing. Don't set this lower then 0.2
	leap_motion.set_smooth_factor(0.5)
```
While the leap motion SDK does a really good job adjusting for the timing difference between two completely separate tracking system I did add some smoothing logic. This adds a small amount of interpolation to the positioning of the hands. I found that very little smoothing is needed, what I initially thoughts was an issue with the wild movement of your head was simply a result of small differences caused by the leap motion device not being perfectly centered on my HMD. 

I've left the option in place for now.

The second option is far more important. Your leap motion is attached to the front of the HMD and is therefor moved a certain distance forward. Seeing I have an Oculus Rift and I am lazy, I've hardcoded the defaults for this to my Rift. If you're using a different HMD you will need to change this as follows:
```
	# our default is 8cm from center between eyes to leap motion, you can change it here if you need to
	var hmd_to_leap = leap_motion.get_hmd_to_leap_motion()
	hmd_to_leap.origin = Vector3(0.0, 0.0, -0.08)
	leap_motion.set_hmd_to_leap_motion(hmd_to_leap)
```
Note that this is a full transform so if your leap motion is attached tilted you can include a rotation as well.
