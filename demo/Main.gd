extends Spatial

var leap_motion = null

func pinched(hand, is_pinched):
	print("Pinched " + str(hand) + " = " + str(is_pinched))

func grabbed(hand, is_grabbed):
	print("Grabbed " + str(hand) + " = " + str(is_grabbed))

func new_hand(hand):
	print("New hand " + str(hand))
	
	# register some events on the new hand
	hand.connect("pinched", self, "pinched")
	hand.connect("grabbed", self, "grabbed")

func about_to_remove_hand(hand):
	print("Removing hand " + str(hand))

func _ready():
	# Create a new leap motion resource
	leap_motion = preload("res://addons/gdleapmotion/gdlm_sensor.gdns").new()
	
	# Tell it what scenes to load when a new hand is detected
	leap_motion.set_left_hand_scene(preload("res://addons/gdleapmotion/scenes/left_hand_with_collisions.tscn"))
	leap_motion.set_right_hand_scene(preload("res://addons/gdleapmotion/scenes/right_hand_with_collisions.tscn"))
	
	# register a few signals
	leap_motion.connect("new_hand", self, "new_hand")
	leap_motion.connect("about_to_remove_hand", self, "about_to_remove_hand")
	
	# add this as a child node to our scene
	add_child(leap_motion)

func _process(delta):
	$FPS.text = String(Performance.get_monitor(Performance.TIME_FPS))

func _on_Quit_pressed():
	get_tree().quit()
