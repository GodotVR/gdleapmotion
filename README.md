# Godot Leap Motion module

This module implements an interface for the Leap motion sensor

Compiling
---------

I've only tested compiling on Windows using Visual Studio 2017.
You need to have Python 2.7 and Scons installed

You also need to download the leap motion SDK here:
https://developer.leapmotion.com/
We are using the LeapC API that was added to the new Orion Beta. You will need version 4.0.0 of this API.

Make sure submodules have been downloaded.
You also need a copy of Godot 3.0.3 or newer, the instructions below assume you have copied the godot.exe into the folder containing this readme.

These instructions also assume you are compiling on a 64bit environment.

If needed, create the godot_api.json file for your version of Godot:
```
godot.exe --gdnative-generate-json-api godot-cpp\godot_headers\api.json
```

Compile the cpp bindings:
```
cd godot-cpp
scons platform=windows generate_bindings=yes
cd ..
```

Compile our module:
```
scons platform=windows leapsdk_path=<path_to_leap_motion_sdk>
```

Add ```target=release``` to both scons commands to build a release version of the module.

(for convenience I'm including my latest 64bit compile but I can't guarantee it will consistantly work)

Using Leap Motion in Godot
--------------------------
There is a small demo application in the demo folder that shows the basics of how the Leap Motion works with Godot as a desktop solution.

The subfolder ```addons``` contains the leap motion driver itself and is also the destination for the compile script. This folder will also be available in the Godot asset library in the near future.
Besides the leap motion driver there are also several support scenes.

The leap motion driver has been implemented as a GDNative driver.

To use it, add a spatial node to your scene, name it something useful like "leap_motion" and then drag the file addons/gdleapmotion/gdlm_sensor.gdns onto the script property of this node.

You'll need to select another node and then select the leap motion node for the interface to update but you will see all the properties listed.

You'll need to set the left hand and right hand scenes to scenes that need to be added when the leap motion starts tracking a hand. There are a couple of example scenes in the scenes subfolder of the add on.

Alternatively you can add ```Leap_Motion.tscn``` or ```Leap_Motion_with_collisions.tscn``` as a subscene to your project. These have preconfigured nodes ready for you.

Using Leap Motion in Godot with a VR headset
--------------------------------------------
There isn't an example for this in this repository. I may add one later or create a separate demo project for this but support for this has been added in the latest build. This does require Godot 3.0.3 or newer to run as this version of Godot has support for frame timing.

Using the Leap Motion in VR takes an approach that is not immediately apparant. It would seem logical to add the Leap Motion node as a child node of the headset. While this will work you'll notice placement issues with the hands when you move your head.

Instead add your node as a child node of your ARVROrigin node. Then turn the ARVR property of the Leap Motion on. The driver will interact with the ARVR system to obtain the location and orientation of the HMD and perform the needed adjustments for hand placement as the sensor data on the leap motion is independently handled from the HMD.

Other properties of the driver
------------------------------
There are a few more properties that you can tweak.

The ```Smooth Factor``` allows you to set a smoothing factor for the tracking. If you experience a lot of jittering setting a lower value will smooth this out but at the price of increased lag.
Don't set this lower then 0.2. A value of 1.0 turns smoothing off.

The ```Hmd To Leap Motion``` transform only applies in ARVR mode and provides the transform with which the leap motion is placed in relation to the HMD.
Seeing I have an Oculus Rift and I am lazy, I've hardcoded the defaults for this to my Rift. If you're using a different HMD you will need to change this. You can do this as follows:
```
	# our default is 8cm from center between eyes to leap motion, you can change it here if you need to
	var hmd_to_leap = leap_motion.get_hmd_to_leap_motion()
	hmd_to_leap.origin = Vector3(0.0, 0.0, -0.08)
	leap_motion.set_hmd_to_leap_motion(hmd_to_leap)
```
Note that this is a full transform so if your leap motion is attached tilted you can include a rotation as well.

The ```Keep Hands For Frames``` setting tells the driver for how many frames you want to keep a hand "alive" after tracking is lost. Especially in VR where you can look away from your hands there can be nasty results when a hand just disappears.

The ```Keep Last Hand``` is an overrule for the previous setting. When turned on the driver will keep atleast one right hand and one left hand "alive" after tracking is lost.

Signals
-------
There are a number of signals that you can connect to on the GDNative module, you can do this as follows in your ```_ready``` function:
```
	$leap_motion.connect("new_hand", self, "new_hand")
	$leap_motion.connect("about_to_remove_hand", self, "about_to_remove_hand")
```
Or by hooking up the signals in the Node tab.

The ```new_hand``` signal is issued when a new subscene for a hand is instantiated and added. This gives you the opertunity to connect to any signals emitted from these scenes.

The ```about_to_remove_hand``` signal is issued right before the subscene for a hand is removed because tracking is lost.

In a future version we'll add further tracking signals.

Pinch and grab
--------------
Besides accurate orientation information the leap motion SDK also provides pinch and grab values that allow for more gesture based interactions. Most of the logic for this resided in our ```hand.gd``` which is the code associated with the subscenes we're adding.

The Leap Motion module will call set_pinch_distance, set_pinch_strength and set_grab_strength on the subscenes so these *must* be implemented.

```pinch_distance``` is the estimated distance between the top of your index finger and thumb.
```pinch_strength``` is the strength of the pinch, a value between 0.0 (finger tips are not touching) and 1.0 (fingers tips are touching)
```grab_strength``` is the strangth of a grab, a value between 0.0 (fist is open) and 1.0 (fist is closed)

When you look at the code in ```hand.gd``` you can see that we do not just make these values available, we also emit signals based on their value. Note specifically the ```pinched``` and ```grabbed``` signals that are implemented here.

About this repository
---------------------
This repository was created by and is maintained by Bastiaan Olij a.k.a. Mux213

You can follow me on twitter for regular updates here:
https://twitter.com/mux213

Videos about my work with Godot including tutorials can by found on my youtube page:
https://www.youtube.com/channel/UCrbLJYzJjDf2p-vJC011lYw
