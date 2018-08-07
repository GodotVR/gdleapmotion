#include "gdlm_sensor.h"

#define PI 3.14159265359

using namespace godot;

void GDLMSensor::_register_methods() {
	Dictionary args;

	args[Variant("hand")] = Variant(Variant::OBJECT);
	register_signal<GDLMSensor>((char *)"new_hand", args);
	register_signal<GDLMSensor>((char *)"about_to_remove_hand", args);

	register_method((char *)"get_is_running", &GDLMSensor::get_is_running);
	register_method((char *)"get_is_connected", &GDLMSensor::get_is_connected);
	register_method((char *)"get_left_hand_scene", &GDLMSensor::get_left_hand_scene);
	register_method((char *)"set_left_hand_scene", &GDLMSensor::set_left_hand_scene);
	register_method((char *)"get_right_hand_scene", &GDLMSensor::get_right_hand_scene);
	register_method((char *)"set_right_hand_scene", &GDLMSensor::set_right_hand_scene);
	register_method((char *)"get_arvr", &GDLMSensor::get_arvr);
	register_method((char *)"set_arvr", &GDLMSensor::set_arvr);
	register_method((char *)"get_smooth_factor", &GDLMSensor::get_smooth_factor);
	register_method((char *)"set_smooth_factor", &GDLMSensor::set_smooth_factor);
	register_method((char *)"get_keep_frames", &GDLMSensor::get_keep_frames);
	register_method((char *)"set_keep_frames", &GDLMSensor::set_keep_frames);
	register_method((char *)"get_keep_last_hand", &GDLMSensor::get_keep_last_hand);
	register_method((char *)"set_keep_last_hand", &GDLMSensor::set_keep_last_hand);
	register_method((char *)"get_hmd_to_leap_motion", &GDLMSensor::get_hmd_to_leap_motion);
	register_method((char *)"set_hmd_to_leap_motion", &GDLMSensor::set_hmd_to_leap_motion);
	register_method((char *)"_physics_process", &GDLMSensor::_physics_process);
	register_method((char *)"get_finger_name", &GDLMSensor::get_finger_name);
	register_method((char *)"get_finger_bone_name", &GDLMSensor::get_finger_bone_name);

	register_property<GDLMSensor, bool>((char *)"arvr", &GDLMSensor::set_arvr, &GDLMSensor::get_arvr, false);
	register_property<GDLMSensor, float>((char *)"smooth_factor", &GDLMSensor::set_smooth_factor, &GDLMSensor::get_smooth_factor, 0.5);
	register_property<GDLMSensor, int>((char *)"keep_hands_for_frames", &GDLMSensor::set_keep_frames, &GDLMSensor::get_keep_frames, 60);
	register_property<GDLMSensor, bool>((char *)"keep_last_hand", &GDLMSensor::set_keep_last_hand, &GDLMSensor::get_keep_last_hand, true);

	register_property<GDLMSensor, String>((char *)"left_hand_scene", &GDLMSensor::set_left_hand_scene, &GDLMSensor::get_left_hand_scene, String());
	register_property<GDLMSensor, String>((char *)"right_hand_scene", &GDLMSensor::set_right_hand_scene, &GDLMSensor::get_right_hand_scene, String());

	// assume rotated by 90 degrees on x axis and -180 on Y and 8cm from center
	Transform htlp;
	htlp.basis = Basis(Vector3(90.0 * PI / 180.0, -180.0 * PI / 180.0, 0.0));
	htlp.origin = Vector3(0.0, 0.0, -0.08);
	register_property<GDLMSensor, Transform>((char *)"hmd_to_leap_motion", &GDLMSensor::set_hmd_to_leap_motion, &GDLMSensor::get_hmd_to_leap_motion, htlp);
}

GDLMSensor::GDLMSensor() {
	printf("Construct leap motion\n");

	clock_synchronizer = NULL;
	lm_thread = NULL;
	is_running = false;
	is_connected = false;
	arvr = false;
	keep_last_hand = true;
	smooth_factor = 0.5;
	last_frame = NULL;
	last_device = NULL;
	last_frame_id = 0;
	keep_hands_for_frames = 60;

	// assume rotated by 90 degrees on x axis and -180 on Y and 8cm from center
	hmd_to_leap_motion.basis = Basis(Vector3(90.0 * PI / 180.0, -180.0 * PI / 180.0, 0.0));
	hmd_to_leap_motion.origin = Vector3(0.0, 0.0, -0.08);

	eLeapRS result = LeapCreateConnection(NULL, &leap_connection);
	if (result == eLeapRS_Success) {
		result = LeapOpenConnection(leap_connection);
		if (result == eLeapRS_Success) {
			set_is_running(true);
			lm_thread = new std::thread(GDLMSensor::lm_main, this);
		} else {
			LeapDestroyConnection(leap_connection);
			leap_connection = NULL;
		}
	}
}

GDLMSensor::~GDLMSensor() {
	printf("Cleanup leap motion\n");

	if (lm_thread != NULL) {
		// if our loop is still running, this will exit it...
		set_is_running(false);
		set_is_connected(false);

		// join up with our thread if it hasn't already completed
		lm_thread->join();

		// and cleanup our thread
		delete lm_thread;
		lm_thread = NULL;
	}

	// our thread should no longer be running so save to clean up..
	if (clock_synchronizer != NULL) {
		LeapDestroyClockRebaser(clock_synchronizer);
		clock_synchronizer = NULL;
	}
	if (leap_connection != NULL) {
		LeapDestroyConnection(leap_connection);
		leap_connection = NULL;
	}

	if (last_device != NULL) {
		// free the space we allocated for our serial number
		free(last_device->serial);

		// free our device
		free(last_device);
		last_device = NULL;
	}

	// and clear this JIC
	last_frame = NULL;

	// finally clean up hands, note that we don't need to free our scenes because they will be removed by Godot.
	while (hand_nodes.size() > 0) {
		GDLMSensor::hand_data *hd = hand_nodes.back();
		hand_nodes.pop_back();
		free(hd);
	}
}

void GDLMSensor::lock() {
	lm_mutex.lock();
}

void GDLMSensor::unlock() {
	lm_mutex.unlock();
}

bool GDLMSensor::get_is_running() {
	bool ret;

	lock();
	ret = is_running;
	unlock();

	return ret;
}

void GDLMSensor::set_is_running(bool p_set) {
	lock();
	is_running = p_set;
	// maybe issue signal?
	unlock();
}

bool GDLMSensor::get_is_connected() {
	bool ret;

	lock();
	ret = is_connected;
	unlock();

	return ret;
}

void GDLMSensor::set_is_connected(bool p_set) {
	lock();
	if (is_connected != p_set) {
		is_connected = p_set;
		if (p_set) {
			LeapCreateClockRebaser(&clock_synchronizer);

			if (arvr) {
				printf("Setting arvr to true\n");
				LeapSetPolicyFlags(leap_connection, eLeapPolicyFlag_OptimizeHMD, 0);
			} else {
				printf("Setting arvr to false\n");
				LeapSetPolicyFlags(leap_connection, 0, eLeapPolicyFlag_OptimizeHMD);
			}
		} else if (clock_synchronizer != NULL) {
			LeapDestroyClockRebaser(clock_synchronizer);
			clock_synchronizer = NULL;
		}

		// maybe issue signal?
	}
	unlock();
}

bool GDLMSensor::wait_for_connection(int timeout, int waittime) {
	int time_passed = 0;
	while (!get_is_connected() && time_passed < timeout) {
		printf("Check...\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(waittime));
		time_passed += waittime;
	}

	return get_is_connected();
}

const LEAP_TRACKING_EVENT *GDLMSensor::get_last_frame() {
	const LEAP_TRACKING_EVENT *ret;

	lock();
	ret = last_frame;
	unlock();

	return ret;
}

void GDLMSensor::set_last_frame(const LEAP_TRACKING_EVENT *p_frame) {
	lock();
	last_frame = p_frame;
	unlock();
}

const LEAP_DEVICE_INFO *GDLMSensor::get_last_device() {
	const LEAP_DEVICE_INFO *ret;

	lock();
	ret = last_device;
	unlock();

	return ret;
}

void GDLMSensor::set_last_device(const LEAP_DEVICE_INFO *p_device) {
	lock();

	if (last_device != NULL) {
		// free the space we allocated for our serial number
		free(last_device->serial);
	} else {
		// allocate memory to store our device in
		last_device = (LEAP_DEVICE_INFO *)malloc(sizeof(*p_device));
	}

	// make a copy of our settings
	*last_device = *p_device;

	// but allocate our own buffer for the serial number
	last_device->serial = (char *)malloc(p_device->serial_length);
	memcpy(last_device->serial, p_device->serial, p_device->serial_length);

	unlock();
}

bool GDLMSensor::get_arvr() const {
	return arvr;
}

void GDLMSensor::set_arvr(bool p_set) {
	lock();

	if (arvr != p_set) {
		arvr = p_set;
		if (is_connected) {
			if (arvr) {
				printf("Setting arvr to true\n");
				LeapSetPolicyFlags(leap_connection, eLeapPolicyFlag_OptimizeHMD, 0);
			} else {
				printf("Setting arvr to false\n");
				LeapSetPolicyFlags(leap_connection, 0, eLeapPolicyFlag_OptimizeHMD);
			}
		}
	}

	unlock();

/*	if (arvr != p_set) {
		if (leap_connection != NULL) {
			printf("Wait for connection\n");
			if (wait_for_connection()) {
				arvr = p_set;
				if (arvr) {
					printf("Setting arvr to true\n");
					LeapSetPolicyFlags(leap_connection, eLeapPolicyFlag_OptimizeHMD, 0);
				} else {
					printf("Setting arvr to false\n");
					LeapSetPolicyFlags(leap_connection, 0, eLeapPolicyFlag_OptimizeHMD);
				}				
			} else {
				printf("Failed to set ARVR\n");
			}
		}
	}*/
}

float GDLMSensor::get_smooth_factor() const {
	return smooth_factor;
}

void GDLMSensor::set_smooth_factor(float p_smooth_factor) {
	smooth_factor = p_smooth_factor;
}

int GDLMSensor::get_keep_frames() const {
	return keep_hands_for_frames;
}

void GDLMSensor::set_keep_frames(int p_keep_frames) {
	keep_hands_for_frames = p_keep_frames;
}

bool GDLMSensor::get_keep_last_hand() const {
	return keep_last_hand;
}

void GDLMSensor::set_keep_last_hand(bool p_keep_hand) {
	keep_last_hand = p_keep_hand;
}

Transform GDLMSensor::get_hmd_to_leap_motion() const {
	return hmd_to_leap_motion;
}

void GDLMSensor::set_hmd_to_leap_motion(Transform p_transform) {
	hmd_to_leap_motion = p_transform;
}

String GDLMSensor::get_left_hand_scene() const {
	return hand_scene_names[0];
}

void GDLMSensor::set_left_hand_scene(String p_resource) {
	if (hand_scene_names[0] != p_resource) {
		hand_scene_names[0] = p_resource;

		// maybe delay loading until we need it?
		hand_scenes[0] = ResourceLoader::load(p_resource);
	}
}

String GDLMSensor::get_right_hand_scene() const {
	return hand_scene_names[1];
}

void GDLMSensor::set_right_hand_scene(String p_resource) {
	if (hand_scene_names[1] != p_resource) {
		hand_scene_names[1] = p_resource;

		// maybe delay loading until we need it?
		hand_scenes[1] = ResourceLoader::load(p_resource);
	}
}

const char *const GDLMSensor::finger[] = {
	"Thumb", "Index", "Middle", "Ring", "Pink"
};

String GDLMSensor::get_finger_name(int p_idx) {
	String finger_name;

	if (p_idx > 0 && p_idx <= 5) {
		finger_name = finger[p_idx - 1];
	}

	return finger_name;
}

const char *const GDLMSensor::finger_bone[] = {
	"Metacarpal", "Proximal", "Intermediate", "Distal"
};

String GDLMSensor::get_finger_bone_name(int p_idx) {
	String finger_bone_name;

	if (p_idx > 0 && p_idx <= 5) {
		finger_bone_name = finger_bone[p_idx - 1];
	}

	return finger_bone_name;
}

void GDLMSensor::update_hand_data(GDLMSensor::hand_data *p_hand_data, LEAP_HAND *p_leap_hand) {
	Array args;

	if (p_hand_data == NULL)
		return;

	if (p_hand_data->scene == NULL)
		return;

	// first pinch distance
	args.push_back(Variant(p_leap_hand->pinch_distance));
	p_hand_data->scene->call("set_pinch_distance", args);

	// then pinch strength
	args.clear();
	args.push_back(Variant(p_leap_hand->pinch_strength));
	p_hand_data->scene->call("set_pinch_strength", args);

	// and grab strength
	args.clear();
	args.push_back(Variant(p_leap_hand->grab_strength));
	p_hand_data->scene->call("set_grab_strength", args);
};

void GDLMSensor::update_hand_position(GDLMSensor::hand_data *p_hand_data, LEAP_HAND *p_leap_hand) {
	Transform hand_transform;

	if (p_hand_data == NULL)
		return;

	if (p_hand_data->scene == NULL)
		return;

	// orientation of our hand using LeapC quarternion
	Quat quat(p_leap_hand->palm.orientation.x, p_leap_hand->palm.orientation.y, p_leap_hand->palm.orientation.z, p_leap_hand->palm.orientation.w);
	Basis base_orientation(quat);

	// apply to our transform
	hand_transform.set_basis(base_orientation);

	// position of our hand
	Vector3 hand_position(
			p_leap_hand->palm.position.x * world_scale,
			p_leap_hand->palm.position.y * world_scale,
			p_leap_hand->palm.position.z * world_scale);
	hand_transform.set_origin(hand_position);

	// get our inverse for positioning the rest of the hand
	Transform hand_inverse = hand_transform.inverse();

	// if in ARVR mode we should xform this to convert from HMD relative position to Origin world position
	if (arvr) {
		Transform last_transform = p_hand_data->scene->get_transform();
		hand_transform = hmd_transform * hmd_to_leap_motion * hand_transform;

		// leap motions frame interpolation is pretty good but we're going to smooth things out a little bit
		// to stop hands from visible drifting when the user turns his/her head. We can live with the position
		// of the hand being a few frames behind
		hand_transform.origin = last_transform.origin.linear_interpolate(hand_transform.origin, smooth_factor);
	};

	// and apply
	p_hand_data->scene->set_transform(hand_transform);

	// lets parse our digits
	for (int d = 0; d < 5; d++) {
		LEAP_DIGIT *digit = &p_leap_hand->digits[d];
		LEAP_BONE *bone = &digit->bones[0];

		// logic for positioning stuff
		Transform parent_inverse = hand_inverse;
		Vector3 up = Vector3(0.0, 1.0, 0.0);
		Transform bone_pose;

		// Our first bone provides our starting position for our first node
		Vector3 bone_start_pos(
				bone->prev_joint.x * world_scale,
				bone->prev_joint.y * world_scale,
				bone->prev_joint.z * world_scale);
		bone_start_pos = parent_inverse.xform(bone_start_pos);
		bone_pose.origin = bone_start_pos;

		// For now assume order, we may change this to naming, we should cache this, maybe do this when we load our scene.
		Spatial *digit_node = p_hand_data->finger_nodes[d];
		if (digit_node != NULL) {
			// And handle our bones.
			int first_bone = d == 0 ? 1 : 0; // we skip the first bone for our thumb
			for (int b = first_bone; b < 4; b++) {
				bone = &digit->bones[b];

				// We calculate rotation with LeapC's quarternion, I couldn't get this to work right.
				// This gives our rotation in world space which we need to change to the rotation diffence
				// between this node and the previous one. Somehow it gets the axis wrong...
				// Should revisit it some day, if we do this code becomes much simpler :)
				// Quat quat(bone->rotation.x, bone->rotation.y, bone->rotation.z, bone->rotation.w);
				// Basis bone_rotation(quat);

				// We calculate rotation based on our next joint position
				Vector3 bone_pos(
						bone->next_joint.x * world_scale,
						bone->next_joint.y * world_scale,
						bone->next_joint.z * world_scale);

				// transform it based on our last locale
				bone_pos = parent_inverse.xform(bone_pos);
				// remove previous position to get our delta
				bone_pos -= bone_pose.origin;

				// Standard cross normalise with up to create our matrix. Could fail on a 90 degree bend.
				Vector3 axis_z = bone_pos;
				axis_z.normalize();
				Vector3 axis_x = up.cross(axis_z).normalized();
				Vector3 axis_y = axis_z.cross(axis_x).normalized();

				bone_pose.basis.set_axis(0, axis_x);
				bone_pose.basis.set_axis(1, axis_y);
				bone_pose.basis.set_axis(2, axis_z);

				if (digit_node != NULL) {
					// now we can set our parent
					digit_node->set_transform(bone_pose);

					digit_node = p_hand_data->digit_nodes[d][b];
				}

				// and update for next iteration
				parent_inverse = bone_pose.inverse() * parent_inverse;

				// Our next nodes origin...
				bone_pose.origin = Vector3(0.0, 0.0, bone_pos.length());
			}

			// set our last digits transform...
			if (digit_node != NULL) {
				bone_pose.basis = Basis();
				digit_node->set_transform(bone_pose);
			}
		}
	}

	// do we want to do something with the arm?
}

GDLMSensor::hand_data *GDLMSensor::find_hand_by_id(int p_type, uint32_t p_leap_id) {
	for (int h = 0; h < hand_nodes.size(); h++) {
		if ((hand_nodes[h]->type == p_type) && (hand_nodes[h]->leap_id == p_leap_id)) {
			return hand_nodes[h];
		}
	}

	return NULL;
}

GDLMSensor::hand_data *GDLMSensor::find_unused_hand(int p_type) {
	for (int h = 0; h < hand_nodes.size(); h++) {
		// note, unused_frames must be bigger then 0, else its just that we've reset this.
		if ((hand_nodes[h]->type == p_type) && (hand_nodes[h]->active_this_frame == false) && (hand_nodes[h]->unused_frames > 0)) {
			return hand_nodes[h];
		}
	}

	return NULL;
}

int GDLMSensor::count_hands(int p_type, bool p_active_only) {
	int count = 0;
	for (int h = 0; h < hand_nodes.size(); h++) {
		if (hand_nodes[h]->type == p_type && (hand_nodes[h]->active_this_frame || !p_active_only)) {
			count++;
		}
	}

	return count;
}

GDLMSensor::hand_data *GDLMSensor::new_hand(int p_type, uint32_t p_leap_id) {
	if (hand_scenes[p_type].is_null()) {
		return NULL;
	} else if (!hand_scenes[p_type]->can_instance()) {
		return NULL;
	}

	hand_data *new_hand_data = (hand_data *)malloc(sizeof(hand_data));

	new_hand_data->type = p_type;
	new_hand_data->leap_id = p_leap_id;
	new_hand_data->active_this_frame = true;
	new_hand_data->unused_frames = 0;

	new_hand_data->scene = (Spatial *)hand_scenes[p_type]->instance(); // is it safe to cast like this?
	new_hand_data->scene->set_name(String("Hand ") + String(p_type) + String(" ") + String(p_leap_id));
	owner->add_child(new_hand_data->scene, false);

	for (int d = 0; d < 5; d++) {
		Spatial *node = (Spatial *)new_hand_data->scene->find_node(String(finger[d]), false);
		if (node == NULL) {
			printf("Couldn''t find node %s\n", finger[d]);

			// clear just in case
			new_hand_data->finger_nodes[d] = NULL;
			for (int b = 0; b < 4; b++) {
				new_hand_data->digit_nodes[d][b] = NULL;
			}
		} else {
			new_hand_data->finger_nodes[d] = node;

			int first_bone = 0;
			if (d == 0) {
				// we're one digit short on our thumb...
				new_hand_data->digit_nodes[0][0] = NULL;
				first_bone = 1;
			}
			for (int b = first_bone; b < 4; b++) {
				if (node != NULL) {
					// find our child node...
					char node_name[256];
					sprintf(node_name, "%s_%s", finger[d], finger_bone[b]);
					node = (Spatial *)node->find_node(String(node_name), false);
					if (node == NULL) {
						printf("Couldn''t find node %s\n", node_name);
					}
				}

				// even if node is NULL, assign it, we want to make sure we don't have old pointers...
				new_hand_data->digit_nodes[d][b] = node;
			}
		}
	}

	Array args;
	args.push_back(Variant(new_hand_data->scene));
	owner->emit_signal("new_hand", args);

	return new_hand_data;
}

void GDLMSensor::delete_hand(GDLMSensor::hand_data *p_hand_data) {
	// this should free everything up and invalidate it, no need to do anything more...
	if (p_hand_data->scene != NULL) {
		Array args;
		args.push_back(Variant(p_hand_data->scene));
		owner->emit_signal("about_to_remove_hand", args);

		// hide and then queue free, this will properly destruct our scene and remove it from our tree
		p_hand_data->scene->hide();
		p_hand_data->scene->queue_free();
	}

	free(p_hand_data);
}

// our Godot physics process, runs within the physic thread and is responsible for updating physics related stuff
void GDLMSensor::_physics_process(float delta) {
	LEAP_TRACKING_EVENT *interpolated_frame = NULL;
	const LEAP_TRACKING_EVENT *frame = NULL;
	uint64_t arvr_frame_usec;

	// We're getting our measurements in mm, want them in m
	world_scale = 0.001;

	// get some arvr stuff
	if (arvr) {
		// At this point in time our timing is such that last_process_usec + last_frame_usec = last_commit_usec
		// and it will be the frame previous to the one we're rendering and it will be the frame get_hmd_transform relates to.
		// Once we start running rendering in a separate thread last_commit_usec will still be the last frame
		// but last_process_usec will be newer and get_hmd_transform could be more up to date.
		// last_frame_usec will be an average.
		// We probably should put this whole thing into a mutex with the render thread once the time is right.
		// For now however.... :)

		world_scale *= ARVRServer::get_world_scale();
		hmd_transform = ARVRServer::get_hmd_transform();
		arvr_frame_usec = ARVRServer::get_last_process_usec() + ARVRServer::get_last_frame_usec();
	}

	// update our timing
	if (clock_synchronizer != NULL) {
		uint64_t godot_usec = OS::get_ticks_msec() * 1000; // why does godot not give us usec while it records it, grmbl...
		uint64_t leap_usec = LeapGetNow();
		LeapUpdateRebase(clock_synchronizer, godot_usec, leap_usec);
	}

	// get our frame, either interpolated or latest
	if (arvr && clock_synchronizer != NULL && arvr_frame_usec != 0) {
		// Get our leap motion clock value at the timing on which we expect our hmd_transform to be.
		// This will never be exact science as we do not know how much of a timewarp Oculus/OpenVR has applied..
		int64_t leap_target_usec;
		uint64_t target_frame_size;
		LeapRebaseClock(clock_synchronizer, arvr_frame_usec, &leap_target_usec);

		// we need to allocate the right amount of memory to store our interpolated frame data at our timestamp
		eLeapRS result = LeapGetFrameSize(leap_connection, leap_target_usec, &target_frame_size);
		if (result == eLeapRS_Success) {
			// get some space
			interpolated_frame = (LEAP_TRACKING_EVENT *)malloc((size_t)target_frame_size);
			if (interpolated_frame != NULL) {
				// and lets get our interpolated frame!!
				result = LeapInterpolateFrame(leap_connection, leap_target_usec, interpolated_frame, target_frame_size);
				if (result == eLeapRS_Success) {
					// copy pointer so we can use the same logic as when we call get_last_frame..
					frame = interpolated_frame;
				} else {
					// this is not good... need to add some error handling here.

					// clean up so we exit..
					free(interpolated_frame);
					interpolated_frame = NULL;
				}
			}
		}
	} else {
		// ok lets process our last frame. Note that leap motion says it caches these so I'm assuming they
		// are valid for a few frames.
		frame = get_last_frame();
	}

	// was everything above successful?
	if (frame == NULL) {
		// we don't have a frame yet, or we failed upstairs..
		return;
	} else if (!arvr && (last_frame_id == frame->info.frame_id)) {
		// we already parsed this, no need to do this. In ARVR we may need to do more
		return;
	}

	last_frame_id = frame->info.frame_id;

	// Lets process our frames...

	// Mark all current hand nodes as inactive, we'll mark the ones that are active as we find they are still used
	for (int h = 0; h < hand_nodes.size(); h++) {
		// if its already inactive we don't want to reset unused frames.
		if (hand_nodes[h]->active_this_frame) {
			hand_nodes[h]->active_this_frame = false;
			hand_nodes[h]->unused_frames = 0;
		}
	}

	// process the hands we're getting from leap motion
	for (uint32_t h = 0; h < frame->nHands; h++) {
		LEAP_HAND *hand = &frame->pHands[h];
		int type = hand->type == eLeapHandType_Left ? 0 : 1;

		// see if we already have a scene for this hand
		hand_data *hd = find_hand_by_id(type, hand->id);
		if (hd == NULL) {
			// nope? then see if we can find a hand we lost tracking for
			hd = find_unused_hand(type);
		}
		if (hd == NULL) {
			// nope? time to get a new hand
			hd = new_hand(type, hand->id);
			if (hd != NULL) {
				hand_nodes.push_back(hd);
			}
		}
		if (hd != NULL) {
			// yeah! mark as used and relate to our hand
			hd->active_this_frame = true;
			hd->unused_frames = 0;
			hd->leap_id = hand->id;

			// and update
			update_hand_data(hd, hand);
			update_hand_position(hd, hand);

			// should make sure hand is visible
		}
	}

	// and clean up, in reverse because we may remove entries
	for (int h = hand_nodes.size() - 1; h >= 0; h--) {
		hand_data *hd = hand_nodes[h];

		// not active?
		if (!hd->active_this_frame) {
			hd->unused_frames++;

			// lost tracking for awhile now? remove it unless its the last one
			if (hd->unused_frames > keep_hands_for_frames && (count_hands(hd->type) > 1 || !keep_last_hand)) {
				delete_hand(hd);
				hand_nodes.erase(hand_nodes.begin() + h);
			} else {
				// should make sure hand is invisible
			}
		}
	}

	// free our buffer if we allocated it
	if (interpolated_frame != NULL) {
		free(interpolated_frame);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// All methods below here are running in our thread!!

/** Translates eLeapRS result codes into a human-readable string. */
const char *GDLMSensor::ResultString(eLeapRS r) {
	switch (r) {
		case eLeapRS_Success: return "eLeapRS_Success";
		case eLeapRS_UnknownError: return "eLeapRS_UnknownError";
		case eLeapRS_InvalidArgument: return "eLeapRS_InvalidArgument";
		case eLeapRS_InsufficientResources: return "eLeapRS_InsufficientResources";
		case eLeapRS_InsufficientBuffer: return "eLeapRS_InsufficientBuffer";
		case eLeapRS_Timeout: return "eLeapRS_Timeout";
		case eLeapRS_NotConnected: return "eLeapRS_NotConnected";
		case eLeapRS_HandshakeIncomplete: return "eLeapRS_HandshakeIncomplete";
		case eLeapRS_BufferSizeOverflow: return "eLeapRS_BufferSizeOverflow";
		case eLeapRS_ProtocolError: return "eLeapRS_ProtocolError";
		case eLeapRS_InvalidClientID: return "eLeapRS_InvalidClientID";
		case eLeapRS_UnexpectedClosed: return "eLeapRS_UnexpectedClosed";
		case eLeapRS_UnknownImageFrameRequest: return "eLeapRS_UnknownImageFrameRequest";
		case eLeapRS_UnknownTrackingFrameID: return "eLeapRS_UnknownTrackingFrameID";
		case eLeapRS_RoutineIsNotSeer: return "eLeapRS_RoutineIsNotSeer";
		case eLeapRS_TimestampTooEarly: return "eLeapRS_TimestampTooEarly";
		case eLeapRS_ConcurrentPoll: return "eLeapRS_ConcurrentPoll";
		case eLeapRS_NotAvailable: return "eLeapRS_NotAvailable";
		case eLeapRS_NotStreaming: return "eLeapRS_NotStreaming";
		case eLeapRS_CannotOpenDevice: return "eLeapRS_CannotOpenDevice";
		default: return "unknown result type.";
	}
}

void GDLMSensor::handleConnectionEvent(const LEAP_CONNECTION_EVENT *connection_event) {
	// log..
	printf("LeapMotion - connected to leap motion\n");

	// update our status
	set_is_connected(true);
}

void GDLMSensor::handleConnectionLostEvent(const LEAP_CONNECTION_LOST_EVENT *connection_lost_event) {
	// update our status
	set_is_connected(false);

	// log...
	printf("LeapMotion - connection lost\n");
}

void GDLMSensor::handleDeviceEvent(const LEAP_DEVICE_EVENT *device_event) {
	// copied from the SDK, just record this, not sure yet if we need to remember any of this..
	LEAP_DEVICE deviceHandle;

	//Open device using LEAP_DEVICE_REF from event struct.
	eLeapRS result = LeapOpenDevice(device_event->device, &deviceHandle);
	if (result != eLeapRS_Success) {
		printf("Could not open device %s.\n", ResultString(result));
		return;
	}

	//Create a struct to hold the device properties, we have to provide a buffer for the serial string
	LEAP_DEVICE_INFO deviceProperties = { sizeof(deviceProperties) };

	// Start with a length of 1 (pretending we don't know a priori what the length is).
	// Currently device serial numbers are all the same length, but that could change in the future
	deviceProperties.serial_length = 1;
	deviceProperties.serial = (char *)malloc(deviceProperties.serial_length);

	// This will fail since the serial buffer is only 1 character long
	// But deviceProperties is updated to contain the required buffer length
	result = LeapGetDeviceInfo(deviceHandle, &deviceProperties);
	if (result == eLeapRS_InsufficientBuffer) {
		//try again with correct buffer size
		deviceProperties.serial = (char *)realloc(deviceProperties.serial, deviceProperties.serial_length);
		result = LeapGetDeviceInfo(deviceHandle, &deviceProperties);
		if (result != eLeapRS_Success) {
			printf("Failed to get device info %s.\n", ResultString(result));
			free(deviceProperties.serial);
			return;
		}
	}

	// log this for now
	printf("LeapMotion - found device %s\n", deviceProperties.serial);

	// remember this device as the last one we interacted with, we're assuming only one is attached for now.
	set_last_device(&deviceProperties);

	free(deviceProperties.serial);
	LeapCloseDevice(deviceHandle);
}

void GDLMSensor::handleDeviceLostEvent(const LEAP_DEVICE_EVENT *device_event) {
	// just log for now
	printf("LeapMotion - lost device\n");
}

void GDLMSensor::handleDeviceFailureEvent(const LEAP_DEVICE_FAILURE_EVENT *device_failure_event) {
	// do something with this
	// device_failure_event->status, device_failure_event->hDevice

	// just log for now
	printf("LeapMotion - device failure %i\n", device_failure_event->status);
}

void GDLMSensor::handleTrackingEvent(const LEAP_TRACKING_EVENT *tracking_event) {
	// !BAS! These events supposedly are cached, but for how long is this pointer still usable?
	// is this a pointer into a round robbin buffer or are we actually better of doing a deep copy?
	// Leap motion sample project does this so for now we trust it.
	// We will indeed be using this data from our main thread during our physics process

	set_last_frame(tracking_event); // support polling tracking data from different thread
}

void GDLMSensor::handleLogEvent(const LEAP_LOG_EVENT *log_event) {
	// just log for now
	char lvl[250];
	switch (log_event->severity) {
		case eLeapLogSeverity_Critical:
			strcpy(lvl, "Critical");
			break;
		case eLeapLogSeverity_Warning:
			strcpy(lvl, "Warning");
			break;
		case eLeapLogSeverity_Information:
			strcpy(lvl, "Information");
			break;
		default:
			strcpy(lvl, "Unknown");
			break;
	}

	printf("LeapMotion - %s - %lli: %s\n", lvl, log_event->timestamp, log_event->message);
}

/** Called by serviceMessageLoop() when a log event is returned by LeapPollConnection(). */
void GDLMSensor::handleLogEvents(const LEAP_LOG_EVENTS *log_events) {
	for (int i = 0; i < (int)(log_events->nEvents); i++) {
		handleLogEvent(&log_events->events[i]);
	}
}

void GDLMSensor::handlePolicyEvent(const LEAP_POLICY_EVENT *policy_event) {
	// just log for now
	printf("LeapMotion - policy event");

	if (policy_event->current_policy & eLeapPolicyFlag_BackgroundFrames) {
		printf(", background frames");
	}
	if (policy_event->current_policy & eLeapPolicyFlag_OptimizeHMD) {
		printf(", optimised for HMD");
	}
	if (policy_event->current_policy & eLeapPolicyFlag_AllowPauseResume) {
		printf(", allow pause and resume");
	}

	printf("\n");
}

void GDLMSensor::handleConfigChangeEvent(const LEAP_CONFIG_CHANGE_EVENT *config_change_event) {
	// do something with this?

	// just log for now
	printf("LeapMotion - config change event\n");
}

void GDLMSensor::handleConfigResponseEvent(const LEAP_CONFIG_RESPONSE_EVENT *config_response_event) {
	// do something with this?

	// just log for now
	printf("LeapMotion - config response event\n");
}

/** Called by serviceMessageLoop() when a point mapping change event is returned by LeapPollConnection(). */
void GDLMSensor::handleImageEvent(const LEAP_IMAGE_EVENT *image_event) {
	// do something with this?

	// just log for now
	printf("LeapMotion - image event\n");
}

/** Called by serviceMessageLoop() when a point mapping change event is returned by LeapPollConnection(). */
void GDLMSensor::handlePointMappingChangeEvent(const LEAP_POINT_MAPPING_CHANGE_EVENT *point_mapping_change_event) {
	// do something with this?

	// just log for now
	printf("LeapMotion - point mapping change event\n");
}

/** Called by serviceMessageLoop() when a point mapping change event is returned by LeapPollConnection(). */
void GDLMSensor::handleHeadPoseEvent(const LEAP_HEAD_POSE_EVENT *head_pose_event) {
	// definately need to implement this once we add an ARVR interface for this.

	// just log for now
	printf("LeapMotion - head pose event\n");
}


void GDLMSensor::lm_main(GDLMSensor *p_sensor) {
	eLeapRS result;
	LEAP_CONNECTION_MESSAGE msg;

	printf("Start thread\n");
	// note, p_sensor should not be destroyed until our thread cleanly exists
	// as that happens in the destruct of our sensor class we should be able to rely on this

	// loop until is_running is set to false by our main process
	while (p_sensor->get_is_running()) {
		// poll connection, this sleeps our thread until we have a message to handle or
		unsigned int timeout = 1000;
		result = LeapPollConnection(p_sensor->leap_connection, timeout, &msg);

		// Handle messages by calling
		switch (msg.type) {
			case eLeapEventType_Connection:
				p_sensor->handleConnectionEvent(msg.connection_event);
				break;
			case eLeapEventType_ConnectionLost:
				p_sensor->handleConnectionLostEvent(msg.connection_lost_event);
				break;
			case eLeapEventType_Device:
				p_sensor->handleDeviceEvent(msg.device_event);
				break;
			case eLeapEventType_DeviceLost:
				p_sensor->handleDeviceLostEvent(msg.device_event);
				break;
			case eLeapEventType_DeviceFailure:
				p_sensor->handleDeviceFailureEvent(msg.device_failure_event);
				break;
			case eLeapEventType_Tracking:
				p_sensor->handleTrackingEvent(msg.tracking_event);
				break;
			case eLeapEventType_ImageComplete:
				// Ignore since 4.0.0
				break;
			case eLeapEventType_ImageRequestError:
				// Ignore since 4.0.0
				break;
			case eLeapEventType_LogEvent:
				p_sensor->handleLogEvent(msg.log_event);
				break;
			case eLeapEventType_Policy:
				p_sensor->handlePolicyEvent(msg.policy_event);
				break;
			case eLeapEventType_ConfigChange:
				p_sensor->handleConfigChangeEvent(msg.config_change_event);
				break;
			case eLeapEventType_ConfigResponse:
				p_sensor->handleConfigResponseEvent(msg.config_response_event);
				break;
			case eLeapEventType_Image:
				p_sensor->handleImageEvent(msg.image_event);
				break;
			case eLeapEventType_PointMappingChange:
				p_sensor->handlePointMappingChangeEvent(msg.point_mapping_change_event);
				break;
			case eLeapEventType_LogEvents:
				p_sensor->handleLogEvents(msg.log_events);
				break;
			case eLeapEventType_HeadPose:
				p_sensor->handleHeadPoseEvent(msg.head_pose_event);
				break;
			default: {
				// ignore
			} break;
		}
	}
}
