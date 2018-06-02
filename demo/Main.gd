extends Spatial

func pinched(hand, is_pinched):
	print("Pinched " + str(hand) + " = " + str(is_pinched))

func grabbed(hand, is_grabbed):
	print("Grabbed " + str(hand) + " = " + str(is_grabbed))

func _process(delta):
	$FPS.text = String(Performance.get_monitor(Performance.TIME_FPS))

func _on_Quit_pressed():
	get_tree().quit()


func _on_Leap_Motion_new_hand(hand):
	print("New hand " + str(hand))
	
	# register some events on the new hand
	hand.connect("pinched", self, "pinched")
	hand.connect("grabbed", self, "grabbed")

func _on_Leap_Motion_about_to_remove_hand(hand):
	print("Removing hand " + str(hand))
