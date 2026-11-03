#include "SpringMath.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace avatar
{
Vector3 constrainSpringTail(Vector3 head, Vector3 tail, float length, Vector3 restDirection)
{
	auto direction = tail - head;
	if (direction.length() < 0.0000001f) direction = restDirection;
	if (direction.length() < 0.0000001f || !std::isfinite(length) || length <= 0)
		throw std::runtime_error("Spring joint requires a finite nonzero bone length");
	return head + direction.normalize() * length;
}

Vector3 integrateSpringTail(Vector3 head, Vector3 current, Vector3 previous, Vector3 restDirection,
	float length, const SpringJointSettings &settings, float deltaTime, float scale)
{
	const auto inertia = deltaTime > 0 ? (current - previous) * (1 - settings.drag) : Vector3();
	const auto stiffness = restDirection * (settings.stiffness * deltaTime * scale);
	const auto gravity = settings.gravityDirection * (settings.gravity * deltaTime * scale);
	return constrainSpringTail(head, current + inertia + stiffness + gravity, length, restDirection);
}

Vector3 collideSpringTail(Vector3 head, Vector3 tail, float length, float hitRadius,
	const SpringWorldCollider &collider, Vector3 restDirection)
{
	const auto segment = collider.end - collider.start;
	const float squaredLength = segment.dot(segment);
	const float along = squaredLength > 0 ? std::max(0.0f, std::min(1.0f, (head - collider.start).dot(segment) / squaredLength)) : 0;
	const auto closest = collider.start + segment * along;
	auto delta = tail - closest;
	const float radius = collider.radius + hitRadius;
	if (delta.dot(delta) >= radius * radius) return tail;
	if (delta.length() < 0.0000001f)
	{
		delta = tail - head;
		if (squaredLength > 0) delta -= segment * (delta.dot(segment) / squaredLength);
		if (delta.length() < 0.0000001f)
			delta = squaredLength > 0 ? segment.cross(std::abs(segment.y) < segment.length() * 0.9f
				? Vector3(0, 1, 0) : Vector3(1, 0, 0)) : restDirection;
	}
	return constrainSpringTail(head, closest + delta.normalize() * radius, length, restDirection);
}
}
