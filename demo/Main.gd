extends Spatial

var leap_motion = null

func _ready():
	# Create a new leap motion resource
	leap_motion = preload("res://bin/gdlm_sensor.gdns").new()
	
	# Tell it what scenes to load when a new hand is detected
	leap_motion.set_left_hand_scene(preload("res://LeapMotion/left_hand_with_collisions.tscn"))
	leap_motion.set_right_hand_scene(preload("res://LeapMotion/right_hand_with_collisions.tscn"))
	
	# add this as a child node to our scene
	add_child(leap_motion)

func _process(delta):
	$FPS.text = String(Performance.get_monitor(Performance.TIME_FPS))

func _on_Quit_pressed():
	get_tree().quit()
