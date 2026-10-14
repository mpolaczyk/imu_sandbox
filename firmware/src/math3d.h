#ifndef IM_MATH3D_H
#define IM_MATH3D_H

#include <math.h>
#include <stdbool.h>

typedef struct
{
	float x;
	float y;
	float z;
} im_vec3;

/* Unit quaternions actively rotate vectors from a source frame to a destination frame. */
typedef struct
{
	float w;
	float x;
	float y;
	float z;
} im_quat;

static inline im_vec3 im_vec3_add(im_vec3 a, im_vec3 b)
{
	return (im_vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline im_vec3 im_vec3_subtract(im_vec3 a, im_vec3 b)
{
	return (im_vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline im_vec3 im_vec3_scale(im_vec3 value, float scale)
{
	return (im_vec3){value.x * scale, value.y * scale, value.z * scale};
}

static inline float im_vec3_dot(im_vec3 a, im_vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline im_vec3 im_vec3_cross(im_vec3 a, im_vec3 b)
{
	return (im_vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static inline float im_vec3_length(im_vec3 value)
{
	return sqrtf(im_vec3_dot(value, value));
}

static inline bool im_vec3_normalize(im_vec3 *value)
{
	const float length = im_vec3_length(*value);
	if (length < 1e-6f)
	{
		return false;
	}
	*value = im_vec3_scale(*value, 1.0f / length);
	return true;
}

static inline im_quat im_quat_identity(void)
{
	return (im_quat){1.0f, 0.0f, 0.0f, 0.0f};
}

static inline im_quat im_quat_add(im_quat a, im_quat b)
{
	return (im_quat){a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline im_quat im_quat_scale(im_quat value, float scale)
{
	return (im_quat){value.w * scale, value.x * scale, value.y * scale, value.z * scale};
}

static inline im_quat im_quat_multiply(im_quat a, im_quat b)
{
	return (im_quat){
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
		a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
		a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

static inline im_quat im_quat_conjugate(im_quat value)
{
	return (im_quat){value.w, -value.x, -value.y, -value.z};
}

static inline bool im_quat_normalize(im_quat *value)
{
	const float norm = sqrtf(value->w * value->w + value->x * value->x + value->y * value->y + value->z * value->z);
	if (norm < 1e-6f)
	{
		return false;
	}
	const float inverse_norm = 1.0f / norm;
	*value = im_quat_scale(*value, inverse_norm);
	return true;
}

static inline im_quat im_quat_from_unit_vectors(im_vec3 from, im_vec3 to)
{
	const float dot = im_vec3_dot(from, to);
	if (dot < -0.999999f)
	{
		const im_vec3 basis = fabsf(from.x) < 0.9f ? (im_vec3){1.0f, 0.0f, 0.0f} : (im_vec3){0.0f, 1.0f, 0.0f};
		im_vec3 axis = im_vec3_cross(from, basis);
		im_vec3_normalize(&axis);
		return (im_quat){0.0f, axis.x, axis.y, axis.z};
	}

	const im_vec3 axis = im_vec3_cross(from, to);
	im_quat result = {1.0f + dot, axis.x, axis.y, axis.z};
	im_quat_normalize(&result);
	return result;
}

static inline im_vec3 im_quat_rotate(im_quat rotation, im_vec3 value)
{
	const im_vec3 vector = {rotation.x, rotation.y, rotation.z};
	const im_vec3 twice_cross = im_vec3_scale(im_vec3_cross(vector, value), 2.0f);
	return im_vec3_add(value, im_vec3_add(im_vec3_scale(twice_cross, rotation.w), im_vec3_cross(vector, twice_cross)));
}

static inline im_vec3 im_quat_rotate_inverse(im_quat rotation, im_vec3 value)
{
	return im_quat_rotate(im_quat_conjugate(rotation), value);
}

#endif