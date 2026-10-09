#include <math.h>
#include <zephyr/kernel.h>

#include "fusion.h"
#include "imu.h"

#define GRAVITY 9.80665f
#define FUSION_DT (1.0f / IM_IMU_SAMPLE_RATE_HZ)
// Mahony proportional gain (rad/s per unit error): higher trusts the accelerometer more
#define FUSION_KP 1.0f
// Accelerometer is used for tilt correction only while |a| is within this of 1 g
#define ACCEL_TRUST_TOL (0.1f * GRAVITY)
// Device counts as stationary when the bias-corrected gyro is below this
#define STILL_GYRO_RAD_S 0.03f
#define STILL_ACCEL_TOL (0.05f * GRAVITY)
#define STILL_SAMPLES (IM_IMU_SAMPLE_RATE_HZ / 4)
#define BIAS_ALPHA 0.002f
// Output rates below this while stationary are suppressed
#define STILL_DEADZONE_RAD_S 0.004f
// Startup calibration: average this many still samples; any motion restarts it
#define CAL_SAMPLES IM_IMU_SAMPLE_RATE_HZ
#define CAL_MOTION_RAD_S 0.05f

static bool s_cal_done;
static uint32_t s_cal_count;
static float s_cal_sum[3];
// Orientation (body to world), gravity-referenced; yaw is gyro-only
static float s_q0 = 1.0f, s_q1, s_q2, s_q3;
static bool s_q_init;
static float s_bias[3];
static uint32_t s_still_count;

static void s_init_from_accel(float ax, float ay, float az)
{
	const float n = sqrtf(ax * ax + ay * ay + az * az);
	if (n < 1e-3f)
	{
		return;
	}
	ax /= n; ay /= n; az /= n;
	// Shortest rotation taking (0,0,1) to the measured up direction: q = (1 + az, -ay, ax, 0)
	float w = 1.0f + az;
	float x = -ay, y = ax;
	const float qn = sqrtf(w * w + x * x + y * y);
	if (qn < 1e-6f)
	{
		w = 0.0f; x = 1.0f; y = 0.0f; // upside down
	}
	else
	{
		w /= qn; x /= qn; y /= qn;
	}
	s_q0 = w; s_q1 = x; s_q2 = y; s_q3 = 0.0f;
	s_q_init = true;
}

// Gyro in rad/s (bias removed), accel in m/s^2
static void s_update_orientation(float gx, float gy, float gz, float ax, float ay, float az, float a_norm)
{
	if (fabsf(a_norm - GRAVITY) < ACCEL_TRUST_TOL)
	{
		// Estimated up direction in the body frame
		const float vx = 2.0f * (s_q1 * s_q3 - s_q0 * s_q2);
		const float vy = 2.0f * (s_q0 * s_q1 + s_q2 * s_q3);
		const float vz = s_q0 * s_q0 - s_q1 * s_q1 - s_q2 * s_q2 + s_q3 * s_q3;
		ax /= a_norm; ay /= a_norm; az /= a_norm;
		gx += FUSION_KP * (ay * vz - az * vy);
		gy += FUSION_KP * (az * vx - ax * vz);
		gz += FUSION_KP * (ax * vy - ay * vx);
	}
	const float h = 0.5f * FUSION_DT;
	const float q0 = s_q0, q1 = s_q1, q2 = s_q2, q3 = s_q3;
	s_q0 += h * (-q1 * gx - q2 * gy - q3 * gz);
	s_q1 += h * (q0 * gx + q2 * gz - q3 * gy);
	s_q2 += h * (q0 * gy - q1 * gz + q3 * gx);
	s_q3 += h * (q0 * gz + q1 * gy - q2 * gx);
	const float n = 1.0f / sqrtf(s_q0 * s_q0 + s_q1 * s_q1 + s_q2 * s_q2 + s_q3 * s_q3);
	s_q0 *= n; s_q1 *= n; s_q2 *= n; s_q3 *= n;
}

bool im_fusion_update(float gx, float gy, float gz, float ax, float ay, float az, struct im_fusion_rates *out)
{
	const float a_norm = sqrtf(ax * ax + ay * ay + az * az);
	if (!s_q_init)
	{
		s_init_from_accel(ax, ay, az);
	}

	// Initial bias calibration: device must lie still for CAL_SAMPLES
	if (!s_cal_done)
	{
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
		}
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
		s_bias[0] += BIAS_ALPHA * (gx - s_bias[0]);
		s_bias[1] += BIAS_ALPHA * (gy - s_bias[1]);
		s_bias[2] += BIAS_ALPHA * (gz - s_bias[2]);
	}

	s_update_orientation(cx, cy, cz, ax, ay, az, a_norm);

	// Rates about gravity (yaw) and about the horizontal part of body Y (pitch): independent of roll
	const float ux = 2.0f * (s_q1 * s_q3 - s_q0 * s_q2);
	const float uy = 2.0f * (s_q0 * s_q1 + s_q2 * s_q3);
	const float uz = s_q0 * s_q0 - s_q1 * s_q1 - s_q2 * s_q2 + s_q3 * s_q3;
	float yaw_rate = cx * ux + cy * uy + cz * uz;
	const float px = -uy * ux;
	const float py = 1.0f - uy * uy;
	const float pz = -uy * uz;
	const float pn = sqrtf(px * px + py * py + pz * pz);
	float pitch_rate = cy;
	if (pn > 0.2f) // near vertical the axis is ill-defined; fall back to raw Y
	{
		pitch_rate = (cx * px + cy * py + cz * pz) / pn;
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
	return true;
}
