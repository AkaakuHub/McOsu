#ifndef MCOSU_SPRING_MATH_H
#define MCOSU_SPRING_MATH_H

#include "SpringDefinition.h"

namespace avatar
{
struct SpringWorldCollider { Vector3 start; Vector3 end; float radius; };
Vector3 constrainSpringTail(Vector3 head, Vector3 tail, float length, Vector3 restDirection);
Vector3 integrateSpringTail(Vector3 head, Vector3 current, Vector3 previous, Vector3 restDirection,
	float length, const SpringJointSettings &settings, float deltaTime, float scale);
Vector3 collideSpringTail(Vector3 head, Vector3 tail, float length, float hitRadius,
	const SpringWorldCollider &collider, Vector3 restDirection);
}

#endif
