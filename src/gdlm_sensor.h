#ifndef GDLM_SENSOR_H
#define GDLM_SENSOR_H

#include <Godot.hpp>
#include <GlobalConstants.hpp>
#include <PackedScene.hpp>
#include <Transform.hpp>
#include <Spatial.hpp>
#include <Skeleton.hpp>
#include <thread>
#include <mutex>

// include leap motion library
#include <LeapC.h>

namespace godot {

#define MAX_HANDS 4

class GDLMSensor : public godot::GodotScript<Spatial> {
	GODOT_CLASS(GDLMSensor);

private:
	LEAP_CONNECTION leap_connection;
	const LEAP_TRACKING_EVENT* last_frame;
	LEAP_DEVICE_INFO* last_device;
	long long int last_frame_id;
	bool is_running;
	bool is_connected;

	std::thread * lm_thread;
	std::mutex lm_mutex;

	// some handy things for defining our hands
	static const char * const finger[];
	static const char * const finger_bone[];

	struct hand_data {
		int type; // 0 = left, 1 = right
		uint32_t leap_id; // ID in leap
		Spatial *scene;
		Spatial *finger_nodes[5]; // the root nodes for each finger
		Spatial *digit_nodes[5][4]; // nodes for each digit
	}; 

	// hands as 0 (left) and 1 (right), we will probably only have one each but just in case...
	float world_scale;
	Ref<PackedScene> hand_scenes[2];
	GDLMSensor::hand_data* hand_nodes[2][MAX_HANDS]; // need to change this to a single array and map to IDs

	GDLMSensor::hand_data* new_hand(int p_type, uint32_t p_leap_id);
	void delete_hand(GDLMSensor::hand_data* p_hand_data);

	// return result state as a string
	const char* ResultString(eLeapRS r);

	// these are handlers for all the messages LeapC sends us
	void handleConnectionEvent(const LEAP_CONNECTION_EVENT *connection_event);
	void handleConnectionLostEvent(const LEAP_CONNECTION_LOST_EVENT *connection_lost_event);
	void handleDeviceEvent(const LEAP_DEVICE_EVENT *device_event);
	void handleDeviceLostEvent(const LEAP_DEVICE_EVENT *device_event);
	void handleDeviceFailureEvent(const LEAP_DEVICE_FAILURE_EVENT *device_failure_event);
	void handleTrackingEvent(const LEAP_TRACKING_EVENT *tracking_event);
	void handleImageCompleteEvent(const LEAP_IMAGE_COMPLETE_EVENT *image_complete_event);
	void handleImageRequestErrorEvent(const LEAP_IMAGE_FRAME_REQUEST_ERROR_EVENT *image_request_error_event);
	void handleLogEvent(const LEAP_LOG_EVENT *log_event);
	void handlePolicyEvent(const LEAP_POLICY_EVENT *policy_event);
	void handleConfigChangeEvent(const LEAP_CONFIG_CHANGE_EVENT *config_change_event);
	void handleConfigResponseEvent(const LEAP_CONFIG_RESPONSE_EVENT *config_response_event);

protected:
	void lock();
	void unlock();

	const LEAP_TRACKING_EVENT* get_last_frame();
	void set_last_frame(const LEAP_TRACKING_EVENT* p_frame);
	const LEAP_DEVICE_INFO* get_last_device();
	void set_last_device(const LEAP_DEVICE_INFO* p_device);

	void set_is_running(bool p_set);
	void set_is_connected(bool p_set);

	void update_hand_position(GDLMSensor::hand_data* p_hand_data, LEAP_HAND* p_lead_hand);

public:
	static void _register_methods();
	static void lm_main(GDLMSensor *p_sensor);

	bool get_is_running();
	bool get_is_connected();

	String get_finger_name(int p_idx);
	String get_finger_bone_name(int p_idx);

	GDLMSensor();
	~GDLMSensor();

	void set_left_hand_scene(Ref<PackedScene> p_resource);
	void set_right_hand_scene(Ref<PackedScene> p_resource);
	void _physics_process(float delta);
};

}

#endif /* !GDLM_SENSOR_H */
