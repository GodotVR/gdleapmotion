#include "gdlm_sensor.h"

using namespace godot;

void GDLMSensor::_register_methods() {
	register_method((char *)"get_is_running", &GDLMSensor::get_is_running);
	register_method((char *)"get_is_connected", &GDLMSensor::get_is_connected);
	register_method((char *)"set_left_hand_scene", &GDLMSensor::set_left_hand_scene);
	register_method((char *)"set_right_hand_scene", &GDLMSensor::set_right_hand_scene);
	register_method((char *)"get_arvr", &GDLMSensor::get_arvr);
	register_method((char *)"set_arvr", &GDLMSensor::set_arvr);
	register_method((char *)"_physics_process", &GDLMSensor::_physics_process);
	register_method((char *)"get_finger_name", &GDLMSensor::get_finger_name);
	register_method((char *)"get_finger_bone_name", &GDLMSensor::get_finger_bone_name);
}

GDLMSensor::GDLMSensor() {
	lm_thread = NULL;
	is_running = false;
	is_connected = false;
	arvr = false;
	last_frame = NULL;
	last_device = NULL;
	last_frame_id = 0;

	for (int hs = 0; hs < 2; hs++) {
		for (int h = 0; h < MAX_HANDS; h++) {
			hand_nodes[hs][h] = NULL;
		}
	}

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

	// finally clean up hands
	for (int hs = 0; hs < 2; hs++) {
		for (int h = 0; h < MAX_HANDS; h++) {
			if (hand_nodes[hs][h] != NULL) {
				// destroying our parent node should already free the children so assume that happens automatically
				// remove_child(hand_nodes[hs][h]);
				// delete hand_nodes[hs][h];

				hand_nodes[hs][h] = NULL;
			}
		}
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
	is_connected = p_set;
	// maybe issue signal?
	unlock();
}

bool GDLMSensor::wait_for_connection() {
	while (!get_is_connected()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	return get_is_connected();
}

const LEAP_TRACKING_EVENT* GDLMSensor::get_last_frame() {
	const LEAP_TRACKING_EVENT* ret;

	lock();
	ret = last_frame;
	unlock();

	return ret;
}

void GDLMSensor::set_last_frame(const LEAP_TRACKING_EVENT* p_frame) {
	lock();
	last_frame = p_frame;
	unlock();
}

const LEAP_DEVICE_INFO* GDLMSensor::get_last_device() {
	const LEAP_DEVICE_INFO* ret;

	lock();
	ret = last_device;
	unlock();

	return ret;
}

void GDLMSensor::set_last_device(const LEAP_DEVICE_INFO* p_device) {
	lock();

	if (last_device != NULL) {
		// free the space we allocated for our serial number
		free(last_device->serial);
	} else {
		// allocate memory to store our device in
		last_device = (LEAP_DEVICE_INFO *) malloc(sizeof(*p_device));
	}

	// make a copy of our settings
	*last_device = *p_device;

	// but allocate our own buffer for the serial number
	last_device->serial = (char *) malloc(p_device->serial_length);
	memcpy(last_device->serial, p_device->serial, p_device->serial_length);

	unlock();
}

bool GDLMSensor::get_arvr() {
	return arvr;
}

void GDLMSensor::set_arvr(bool p_set) {
	if (arvr != p_set) {
		if (leap_connection != NULL) {
			arvr = p_set;
			wait_for_connection();
			if (arvr) {
				printf("Setting arvr to true\n");
				LeapSetPolicyFlags(leap_connection, eLeapPolicyFlag_OptimizeHMD, 0);
			} else {
				printf("Setting arvr to false\n");
				LeapSetPolicyFlags(leap_connection, 0, eLeapPolicyFlag_OptimizeHMD);
			}
		}
	}
}

void GDLMSensor::set_left_hand_scene(Ref<PackedScene> p_resource) {
	hand_scenes[0] = p_resource;
}

void GDLMSensor::set_right_hand_scene(Ref<PackedScene> p_resource) {
	hand_scenes[1] = p_resource;
}

const char * const GDLMSensor::finger[] = {
	"Thumb", "Index", "Middle", "Ring", "Pink"
};

String GDLMSensor::get_finger_name(int p_idx) {
	String finger_name;

	if (p_idx > 0 && p_idx <= 5) {
		finger_name = finger[p_idx - 1];
	}

	return finger_name;
}

const char * const GDLMSensor::finger_bone[] = {
	"Metacarpal", "Proximal", "Intermediate", "Distal"
};

String GDLMSensor::get_finger_bone_name(int p_idx) {
	String finger_bone_name;

	if (p_idx > 0 && p_idx <= 5) {
		finger_bone_name = finger_bone[p_idx - 1];
	}

	return finger_bone_name;
}

void GDLMSensor::update_hand_position(GDLMSensor::hand_data* p_hand_data, LEAP_HAND* p_lead_hand) {
	Transform hand_transform;

	if (p_hand_data == NULL)
		return;

	if (p_hand_data->scene == NULL)
		return;

	// orientation of our hand using LeapC quarternion
	Quat quat(p_lead_hand->palm.orientation.x, p_lead_hand->palm.orientation.y, p_lead_hand->palm.orientation.z, p_lead_hand->palm.orientation.w);
	Basis base_orientation(quat);

	// apply to our transform
	hand_transform.set_basis(base_orientation);

	// position of our hand
	Vector3 hand_position(
		p_lead_hand->palm.position.x * world_scale, 
		p_lead_hand->palm.position.y * world_scale, 
		p_lead_hand->palm.position.z * world_scale
	);
	hand_transform.set_origin(hand_position);

	// get our inverse for positioning the rest of the hand
	Transform hand_inverse = hand_transform.inverse();

	// if in ARVR mode we should xform this to convert from HMD relative position to Origin world position

	// and apply
	p_hand_data->scene->set_transform(hand_transform);

	// lets parse our digits
	for (int d = 0; d < 5; d++) {
		LEAP_DIGIT* digit = &p_lead_hand->digits[d];
		LEAP_BONE* bone = &digit->bones[0];

		// logic for positioning stuff
		Transform parent_inverse = hand_inverse;
		Vector3 up = Vector3(0.0, 1.0, 0.0);
		Transform bone_pose;

		// Our first bone provides our starting position for our first node 
		Vector3 bone_start_pos(
			bone->prev_joint.x * world_scale,
			bone->prev_joint.y * world_scale,
			bone->prev_joint.z * world_scale
		);
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
					bone->next_joint.z * world_scale
				);

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

GDLMSensor::hand_data* GDLMSensor::new_hand(int p_type, uint32_t p_leap_id) {
	hand_data* new_hand_data = (hand_data *) malloc(sizeof(hand_data));

	new_hand_data->type = p_type;
	new_hand_data->leap_id = p_leap_id;

	new_hand_data->scene = (Spatial *) hand_scenes[p_type]->instance(); // is it safe to cast like this?
	new_hand_data->scene->set_name(String("Hand ") + String(p_type) + String(" ") + String(p_leap_id));
	owner->add_child(new_hand_data->scene, false);

	for (int d = 0; d < 5; d++) {
		Spatial* node = (Spatial *)new_hand_data->scene->find_node(String(finger[d]), false);
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
					node = (Spatial *) node->find_node(String(node_name), false);
					if (node == NULL) {
						printf("Couldn''t find node %s\n", node_name);
					}
				}

				// even if node is NULL, assign it, we want to make sure we don't have old pointers...
				new_hand_data->digit_nodes[d][b] = node;
			}
		}
	}

	return new_hand_data;
}

void GDLMSensor::delete_hand(GDLMSensor::hand_data* p_hand_data){
	// this should free everything up and invalidate it, no need to do anything more...
	if (p_hand_data->scene != NULL) {
		// hide and then queue free, this will properly destruct our scene and remove it from our tree
		p_hand_data->scene->hide();
		p_hand_data->scene->queue_free();
	}

	free(p_hand_data);
}

// our Godot physics process, runs within the physic thread and is responsible for updating physics related stuff
void GDLMSensor::_physics_process(float delta) {
	world_scale = 0.001; // We're getting our measurements in mm, want them in m

	// if we're in ARVR mode we should get our ARVR world scale and multiply it with our scale here

	// ok lets process our last frame. Note that leap motion says it caches these so I'm assuming they 
	// are valid for a few frames. 
	const LEAP_TRACKING_EVENT* frame = get_last_frame();
	if (frame == NULL) {
		// we don't have a frame yet
		return;
	} else if (last_frame_id == frame->info.frame_id) {
		// already processed this, do we really want to skip this or are we going to timewarp this?
	}

	last_frame_id = frame->info.frame_id;

	// lets process our frames...
	// printf("Frame %lli with %i hands.\n", (long long int)frame->info.frame_id, frame->nHands);

	// we check our left hands and right hands separately or we'll end up doing crazy stuff with our scenes
	for (int hs = 0; hs < 2; hs++) {
		if (hand_scenes[hs].is_null()) {
			// no scene has been setup so...
			// printf("LeapMotion - no scene for hand %i\n", hs);
			return;
		} else if (!hand_scenes[hs]->can_instance()) {
			printf("LeapMotion - Hand scene for hand %i can't be instantiated!\n", hs);
			return;
		} else {
			int h = 0;

			// rewrite this to use hand->id in combination with hand->type to find it in
			// a vector or something.  

			for (uint32_t fh = 0; fh < frame->nHands; fh++) {
				LEAP_HAND* hand = &frame->pHands[fh];

				if (h >= MAX_HANDS) {
					// we already have too many...
				} else if (hand->type == (hs == 0 ? eLeapHandType_Left : eLeapHandType_Right)) {
					if (hand_nodes[hs][h] == NULL) {
						hand_nodes[hs][h] = new_hand(hs, hand->id);
					}

					update_hand_position(hand_nodes[hs][h], hand);

					h++;
				}
			}

			// remove hands we're no longer tracking, possibly add a grace period. Sometimes a hands tracking
			// is briefly lost and we could reuse it
			while (h < MAX_HANDS) {
				if (hand_nodes[hs][h] != NULL) {
					delete_hand(hand_nodes[hs][h]);
					hand_nodes[hs][h] = NULL;
				}

				h++;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// All methods below here are running in our thread!!

/** Translates eLeapRS result codes into a human-readable string. */
const char* GDLMSensor::ResultString(eLeapRS r) {
	switch(r){
		case eLeapRS_Success:                  return "eLeapRS_Success";
		case eLeapRS_UnknownError:             return "eLeapRS_UnknownError";
		case eLeapRS_InvalidArgument:          return "eLeapRS_InvalidArgument";
		case eLeapRS_InsufficientResources:    return "eLeapRS_InsufficientResources";
		case eLeapRS_InsufficientBuffer:       return "eLeapRS_InsufficientBuffer";
		case eLeapRS_Timeout:                  return "eLeapRS_Timeout";
		case eLeapRS_NotConnected:             return "eLeapRS_NotConnected";
		case eLeapRS_HandshakeIncomplete:      return "eLeapRS_HandshakeIncomplete";
		case eLeapRS_BufferSizeOverflow:       return "eLeapRS_BufferSizeOverflow";
		case eLeapRS_ProtocolError:            return "eLeapRS_ProtocolError";
		case eLeapRS_InvalidClientID:          return "eLeapRS_InvalidClientID";
		case eLeapRS_UnexpectedClosed:         return "eLeapRS_UnexpectedClosed";
		case eLeapRS_UnknownImageFrameRequest: return "eLeapRS_UnknownImageFrameRequest";
		case eLeapRS_UnknownTrackingFrameID:   return "eLeapRS_UnknownTrackingFrameID";
		case eLeapRS_RoutineIsNotSeer:         return "eLeapRS_RoutineIsNotSeer";
		case eLeapRS_TimestampTooEarly:        return "eLeapRS_TimestampTooEarly";
		case eLeapRS_ConcurrentPoll:           return "eLeapRS_ConcurrentPoll";
		case eLeapRS_NotAvailable:             return "eLeapRS_NotAvailable";
		case eLeapRS_NotStreaming:             return "eLeapRS_NotStreaming";
		case eLeapRS_CannotOpenDevice:         return "eLeapRS_CannotOpenDevice";
		default:                               return "unknown result type.";
	}
}

void GDLMSensor::handleConnectionEvent(const LEAP_CONNECTION_EVENT *connection_event) {
	// update our status
	set_is_connected(true);

	// log..
	printf("LeapMotion - connected to leap motion\n");
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
	printf("LeapMotion - found device %s\n",deviceProperties.serial);

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

void GDLMSensor::handleImageCompleteEvent(const LEAP_IMAGE_COMPLETE_EVENT *image_complete_event) {
	// do something with this?

	// just log for now
	printf("LeapMotion - image complete event\n");
}

void GDLMSensor::handleImageRequestErrorEvent(const LEAP_IMAGE_FRAME_REQUEST_ERROR_EVENT *image_request_error_event) {
	// do something with this?

	// just log for now
	printf("LeapMotion - image request error event\n");
}

void GDLMSensor::handleLogEvent(const LEAP_LOG_EVENT *log_event) {
	// just log for now
	printf("LeapMotion - Error %i - %lli: %s\n", log_event->Severity, log_event->Timestamp, log_event->Message);
}

void GDLMSensor::handlePolicyEvent(const LEAP_POLICY_EVENT *policy_event) {
	// just log for now
	printf("LeapMotion - policy event");

	if (policy_event->current_policy & eLeapPolicyFlag_BackgroundFrames) {
		printf(", background frames");
	}
	if (policy_event->current_policy & eLeapPolicyFlag_OptimizeHMD ) {
		printf(", optimised for HMD");
	}
	if (policy_event->current_policy & eLeapPolicyFlag_AllowPauseResume  ) {
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

void GDLMSensor::lm_main(GDLMSensor *p_sensor) {
	eLeapRS result;
	LEAP_CONNECTION_MESSAGE msg;

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
				p_sensor->handleImageCompleteEvent(msg.image_complete_event);
				break;
			case eLeapEventType_ImageRequestError:
				p_sensor->handleImageRequestErrorEvent(msg.image_request_error_event);
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
			default: {
				// ignore
			} break;
		}
	}
}

