#ifndef VEC3_H
#define VEC3_H
#include <cmath>

struct vec3 {
    float x, y, z;

    vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

	vec3 operator-(vec3 b) {
		return { x - b.x, y - b.y, z - b.z };
	}
	vec3 operator-(vec3* b) {
		return { x - b->x, y - b->y, z - b->z };
	}

	vec3 operator+(vec3 b) {
		return { x + b.x, y + b.y, z + b.z };
	}

	bool isNotZero() {
		return 
			floor(x) != 0.0 &&
			floor(y) != 0.0 &&
			floor(z) != 0.0;
	}

    float length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	vec3 normalized() const {
		float len = length();
		if (len == 0.f) return vec3();
		return vec3(x / len, y / len, z / len);
	}
};

vec3 GetForwardVector(vec3 &pos1, vec3 &pos2) {
	auto direction = pos2 - pos1;
	return direction.normalized();
}

#endif
