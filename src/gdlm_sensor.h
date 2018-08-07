#ifndef GDLM_SENSOR_H
#define GDLM_SENSOR_H

#include <ARVRInterface.hpp>
#include <ARVRServer.hpp>
#include <Array.hpp>
#include <Dictionary.hpp>
#include <GlobalConstants.hpp>
#include <Godot.hpp>
#include <OS.hpp>
#include <PackedScene.hpp>
#include <ResourceLoader.hpp>
#include <Skeleton.hpp>
#include <Spatial.hpp>
#include <Transform.hpp>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

// include leap motion library
#include <LeapC.h>

namespace godot {

class GDLMSensor : public godot::GodotScript<Spatial> {
	GODOT_CLASS(GDLMSensor);

private:
	LEAP_CONNECTION leap_connection;
	LEAP_CLOCK_REBASER clock_synchronizer;
	const LEAP_TRACKING_EVENT *last_frame;
	LEAP_DEVICE_INFO *last_device;
	long long int last_frame_id;
	bool is_running;
	bool is_connected;
	bool arvr;
	bool keep_last_hand;
	float smooth_factor;
	int keep_hands_for_frames;
	Transform hmd_transform; /* for ARVR only, transform of our primary HMD */
	Transform hmd_to_leap_motion; /* for ARVR only, transform to adjust leap motion */

	std::thread *lm_thread;
	std::mutex lm_mutex;

	// some handy things for defining our hands
	static const char *const finger[];
	static const char *const finger_bone[];

	struct hand_data {
		int type; // 0 = left, 1 = right
		uint32_t leap_id; // ID in leap
		bool active_this_frame; // is this hand active this frame?
		uint32_t unused_frames; // number of frames since we lost tracking of this hand
		Spatial *scene;
		Spatial *finger_nodes[5]; // the root nodes for each finger
		Spatial *digit_nodes[5][4]; // nodes for each digit
	};

	// hands as 0 (left) and 1 (right), we will probably only have one each but just in case...
	float world_scale;
	String hand_scene_names[2];
	Ref<PackedScene> hand_scenes[2];
	std::vector<GDLMSensor::hand_data *> hand_nodes;

	GDLMSensor::hand_data *find_hand_by_id(int p_type, uint32_t p_leap_id);
	GDLMSensor::hand_data *find_unused_hand(int p_type);
	int count_hands(int p_type, bool p_active_only = false);
	GDLMSensor::hand_data *new_hand(int p_type, uint32_t p_leap_id);
	void delete_hand(GDLMSensor::hand_data *p_hand_data);

	// return result state as a string
	const char *ResultString(eLeapRS r);

	// these are handlers for all the messages LeapC sends us
	void handleConnectionEvent(const LEAP_CONNECTION_EVENT *connection_event);
	void handleConnectionLostEvent(const LEAP_CONNECTION_LOST_EVENT *connection_lost_event);
	void handleDeviceEvent(const LEAP_DEVICE_EVENT *device_event);
	void handleDeviceLostEvent(const LEAP_DEVICE_EVENT *device_event);
	void handleDeviceFailureEvent(const LEAP_DEVICE_FAILURE_EVENT *device_failure_event);
	void handleTrackingEvent(const LEAP_TRACKING_EVENT *tracking_event);
	void handleLogEvent(const LEAP_LOG_EVENT *log_event);
	void handleLogEvents(const LEAP_LOG_EVENTS *log_events);
	void handlePolicyEvent(const LEAP_POLICY_EVENT *policy_event);
	void handleConfigChangeEvent(const LEAP_CONFIG_CHANGE_EVENT *config_change_event);
	void handleConfigResponseEvent(const LEAP_CONFIG_RESPONSE_EVENT *config_response_event);
	void handleImageEvent(const LEAP_IMAGE_EVENT *image_event);
	void handlePointMappingChangeEvent(const LEAP_POINT_MAPPING_CHANGE_EVENT *point_mapping_change_event);
	void handleHeadPoseEvent(const LEAP_HEAD_POSE_EVENT *head_pose_event);

protected:
	void lock();
	void unlock();

	const LEAP_TRACKING_EVENT *get_last_frame();
	void set_last_frame(const LEAP_TRACKING_EVENT *p_frame);
	const LEAP_DEVICE_INFO *get_last_device();
	void set_last_device(const LEAP_DEVICE_INFO *p_device);

	void set_is_running(bool p_set);
	void set_is_connected(bool p_set);

	bool wait_for_connection(int timeout = 5000, int waittime = 1100);

	void update_hand_data(GDLMSensor::hand_data *p_hand_data, LEAP_HAND *p_leap_hand);
	void update_hand_position(GDLMSensor::hand_data *p_hand_data, LEAP_HAND *p_leap_hand);

public:
	static void _register_methods();
	static void lm_main(GDLMSensor *p_sensor);

	bool get_is_running();
	bool get_is_connected();

	String get_finger_name(int p_idx);
	String get_finger_bone_name(int p_idx);

	bool get_arvr() const;
	void set_arvr(bool p_set);

	float get_smooth_factor() const;
	void set_smooth_factor(float p_smooth_factor);

	int get_keep_frames() const;
	void set_keep_frames(int p_keep_frames);

	bool get_keep_last_hand() const;
	void set_keep_last_hand(bool p_keep_hand);

	Transform get_hmd_to_leap_motion() const;
	void set_hmd_to_leap_motion(Transform p_transform);

	GDLMSensor();
	~GDLMSensor();

	String get_left_hand_scene() const;
	void set_left_hand_scene(String p_resource);
	String get_right_hand_scene() const;
	void set_right_hand_scene(String p_resource);
	void _physics_process(float delta);
};

} // namespace godot

#endif /* !GDLM_SENSOR_H */
