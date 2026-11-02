#ifndef MCOSU_SPRING_DEFINITION_H
#define MCOSU_SPRING_DEFINITION_H

#include "Vectors.h"
#include <cstddef>
#include <vector>

struct cgltf_data;

namespace avatar
{
struct SpringJointSettings
{
	std::size_t node;
	float radius = 0;
	float stiffness = 1;
	float gravity = 0;
	Vector3 gravityDirection = Vector3(0, -1, 0);
	float drag = 0.5f;
};
struct SpringCollider
{
	std::size_t node;
	Vector3 offset;
	Vector3 tail;
	float radius;
};
struct SpringChain
{
	int center = -1;
	std::vector<SpringJointSettings> joints;
	std::vector<std::size_t> colliders;
};
struct SpringDefinition
{
	std::vector<SpringCollider> colliders;
	std::vector<SpringChain> chains;
};
SpringDefinition readSpringDefinition(const cgltf_data &scene);
}

#endif
