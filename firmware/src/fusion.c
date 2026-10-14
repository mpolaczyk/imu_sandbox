#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "fusion.h"
#include "imu.h"
#include "math3d.h"

/*
 * Sensor fusion: gyro + accelerometer -> yaw/pitch rates for the cursor.
 *
 * Orientation is a unit quaternion integrated from the gyro and corrected by the accelerometer
 * (Mahony complementary filter, proportional term only):
 *   - The gyro is smooth and precise short-term, but its bias makes the angle drift.
 *   - The accelerometer measures gravity, which gives an absolute tilt (roll/pitch), but is
 *     noisy and wrong while the device accelerates. It is trusted only when |a| is close to 1 g.
 *   - The error between measured and estimated "up" (cross product) is fed back into the gyro
 *     rate with gain FUSION_KP, which pulls the tilt estimate toward gravity.
 *   - Gravity says nothing about yaw, so yaw is gyro-only; its drift is limited by bias removal.
 *
 * Gyro bias is removed in two steps: an averaged startup calibration (device must lie still),
 * then a slow low-pass update while the device is detected as stationary. Rates under a small
 * deadzone are zeroed while stationary so the cursor does not creep.
 *
 * The output rates are taken about gravity (yaw) and about the horizontal part of body Y (pitch),
 * so the cursor motion does not depend on how the device is rolled.
 *
 * Sensor axes (device in hand with USB port pointing forward), based on stationary measurements:
 * - X-axis: Longitudinally along the board. Positive value away from the USB port.
 * - Y-axis: Laterally across the board. Positive value towards the right side.
 * - Z-axis: Perpendicular to the board. Positive value points up when the board lies flat.
 *
 * Positive rotations follow the right-hand rule:
 * - Roll about +X: +Y rotates toward +Z.
 * - Pitch about +Y: +Z rotates toward +X.
 * - Yaw about +Z: +X rotates toward +Y.
 *
 * TOP VIEW
 *                USB PORT
 *                forward ^
 *                        |
 *             +------------------+
 *             |                  |----> +Y (right)
 *             |      DEVICE      |
 *             |                  |
 *             +------------------+
 *                        |
 *                        v
 *                 +X (away from USB)
 *
 *     +Z points up, toward the viewer.
 */

#define GRAVITY 9.80665f
// Mahony proportional gain (rad/s per unit error): higher trusts the accelerometer more
#define FUSION_KP 1.0f
// Accelerometer is used for tilt correction only while |a| is within this of 1 g
#define ACCEL_TRUST_TOL (0.1f * GRAVITY)
// Device counts as stationary when the bias-corrected gyro is below this
#define STILL_GYRO_RAD_S 0.03f
#define STILL_ACCEL_TOL (0.05f * GRAVITY)
#define STILL_SAMPLES (CONFIG_IM_IMU_SAMPLE_RATE_HZ / 4)
#define BIAS_ALPHA 0.002f
// Output rates below this while stationary are suppressed
#define STILL_DEADZONE_RAD_S 0.004f
// Startup calibration: average this many still samples; any motion restarts it
#define CAL_SAMPLES CONFIG_IM_IMU_SAMPLE_RATE_HZ
#define CAL_MOTION_RAD_S 0.05f

static bool s_cal_done;
static uint32_t s_cal_count;
static float s_cal_sum[3];
// Orientation (body to world), gravity-referenced; yaw is gyro-only
static im_quat s_orientation = {1.0f, 0.0f, 0.0f, 0.0f};
static bool s_q_init;
static float s_bias[3];
static uint32_t s_still_count;

void im_fusion_reset(void)
{
	s_cal_done = false;
	s_cal_count = 0;
	s_cal_sum[0] = 0.0f;
	s_cal_sum[1] = 0.0f;
	s_cal_sum[2] = 0.0f;
	s_orientation = im_quat_identity();
	s_q_init = false;
	s_bias[0] = 0.0f;
	s_bias[1] = 0.0f;
	s_bias[2] = 0.0f;
	s_still_count = 0;
}

static void s_init_from_accel(float ax, float ay, float az)
{
	im_vec3 measured_up = {ax, ay, az};
	if (!im_vec3_normalize(&measured_up))
	{
		return;
	}
	const im_vec3 world_up = {0.0f, 0.0f, 1.0f};
	s_orientation = im_quat_from_unit_vectors(measured_up, world_up);
	s_q_init = true;
}

static im_vec3 s_get_up_in_body(void)
{
	const im_vec3 world_up = {0.0f, 0.0f, 1.0f};
	return im_quat_rotate_inverse(s_orientation, world_up);
}

// Gyro in rad/s (bias removed), accel in m/s^2, elapsed time in seconds
static void s_update_orientation(float gx, float gy, float gz, float ax, float ay, float az, float a_norm, float dt_s)
{
	im_vec3 gyro_rate = {gx, gy, gz};
	if (fabsf(a_norm - GRAVITY) < ACCEL_TRUST_TOL)
	{
		im_vec3 measured_up = {ax, ay, az};
		im_vec3_normalize(&measured_up);
		const im_vec3 estimated_up = s_get_up_in_body();
		const im_vec3 error = im_vec3_cross(measured_up, estimated_up);
		gyro_rate = im_vec3_add(gyro_rate, im_vec3_scale(error, FUSION_KP));
	}
	const float h = 0.5f * dt_s;
	const im_quat angular_rate = {0.0f, gyro_rate.x, gyro_rate.y, gyro_rate.z};
	const im_quat derivative = im_quat_multiply(s_orientation, angular_rate);
	s_orientation = im_quat_add(s_orientation, im_quat_scale(derivative, h));
	if (!im_quat_normalize(&s_orientation))
	{
		s_orientation = im_quat_identity();
	}
}

static void s_update_calibration(float gx, float gy, float gz)
{
	/* This variation-only check can mistake steady rotation for gyro bias. */
#ifndef CONFIG_IM_IMU_SKIP_CALIBRATION_MOTION_CHECK
	if (s_cal_count > 0)
	{
		const float inv = 1.0f / s_cal_count;
		if (fabsf(gx - s_cal_sum[0] * inv) > CAL_MOTION_RAD_S || fabsf(gy - s_cal_sum[1] * inv) > CAL_MOTION_RAD_S ||
		    fabsf(gz - s_cal_sum[2] * inv) > CAL_MOTION_RAD_S)
		{
			s_cal_count = 0;
			s_cal_sum[0] = s_cal_sum[1] = s_cal_sum[2] = 0.0f;
		}
	}
#endif
	s_cal_sum[0] += gx;
	s_cal_sum[1] += gy;
	s_cal_sum[2] += gz;
	if (++s_cal_count >= CAL_SAMPLES)
	{
		for (int i = 0; i < 3; i++)
		{
			s_bias[i] = s_cal_sum[i] / s_cal_count;
		}
		s_cal_done = true;
		printk("IMU fusion calibration complete\n");
	}
}

bool im_fusion_update(float gx, float gy, float gz, float ax, float ay, float az, float dt_s, struct im_fusion_rates *out)
{
	const float a_norm = sqrtf(ax * ax + ay * ay + az * az);
	if (!s_q_init)
	{
		s_init_from_accel(ax, ay, az);
	}

	// Initial bias calibration: device must lie still for CAL_SAMPLES
	if (!s_cal_done)
	{
		s_update_calibration(gx, gy, gz);
		return false;
	}

	// Gyro bias: track the raw gyro while the device is held still
	const float cx = gx - s_bias[0];
	const float cy = gy - s_bias[1];
	const float cz = gz - s_bias[2];
	const bool still = fabsf(cx) < STILL_GYRO_RAD_S &&
			   fabsf(cy) < STILL_GYRO_RAD_S &&
			   fabsf(cz) < STILL_GYRO_RAD_S &&
			   fabsf(a_norm - GRAVITY) < STILL_ACCEL_TOL;
	s_still_count = still ? s_still_count + 1 : 0;
	const bool settled = s_still_count >= STILL_SAMPLES;
	if (settled)
	{
		float bias_alpha = BIAS_ALPHA * dt_s * CONFIG_IM_IMU_SAMPLE_RATE_HZ;
		if (bias_alpha > 1.0f)
		{
			bias_alpha = 1.0f;
		}
		s_bias[0] += bias_alpha * (gx - s_bias[0]);
		s_bias[1] += bias_alpha * (gy - s_bias[1]);
		s_bias[2] += bias_alpha * (gz - s_bias[2]);
	}

	s_update_orientation(cx, cy, cz, ax, ay, az, a_norm, dt_s);

	// Rates about gravity (yaw) and about the horizontal part of body Y (pitch): independent of roll
	const im_vec3 gyro_rate = {cx, cy, cz};
	const im_vec3 up_in_body = s_get_up_in_body();
	float yaw_rate = im_vec3_dot(gyro_rate, up_in_body);
	const im_vec3 body_y = {0.0f, 1.0f, 0.0f};
	const im_vec3 pitch_axis = im_vec3_subtract(body_y, im_vec3_scale(up_in_body, im_vec3_dot(body_y, up_in_body)));
	const float pitch_norm = im_vec3_length(pitch_axis);
	float pitch_rate = cy;
	if (pitch_norm > 0.2f) // near vertical the axis is ill-defined; fall back to raw Y
	{
		pitch_rate = im_vec3_dot(gyro_rate, im_vec3_scale(pitch_axis, 1.0f / pitch_norm));
	}
	if (settled && fabsf(yaw_rate) < STILL_DEADZONE_RAD_S)
	{
		yaw_rate = 0.0f;
	}
	if (settled && fabsf(pitch_rate) < STILL_DEADZONE_RAD_S)
	{
		pitch_rate = 0.0f;
	}
	out->yaw = yaw_rate;
	out->pitch = pitch_rate;
	out->qw = s_orientation.w;
	out->qx = s_orientation.x;
	out->qy = s_orientation.y;
	out->qz = s_orientation.z;
	out->gx = cx;
	out->gy = cy;
	out->gz = cz;
	return true;
}
