#include "Spring/SpringMath.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }

int main()
{
	try
	{
		avatar::SpringJointSettings settings;
		settings.gravity = 1;
		const Vector3 head(0, 0, 0), rest(0, 1, 0), current(0, 1, 0), previous(-0.2f, 1, 0);
		const auto paused = avatar::integrateSpringTail(head, current, previous, rest, 1, settings, 0, 1);
		require((paused - current).length() < 0.000001f, "Zero time advanced inertia");
		const auto moved = avatar::integrateSpringTail(head, current, previous, rest, 1, settings, 1.0f / 60, 1);
		require(moved.x > 0 && std::abs(moved.length() - 1) < 0.000001f, "Inertia or bone length is wrong");
		settings.drag = 1;
		const auto stopped = avatar::integrateSpringTail(head, current, previous, rest, 1, settings, 1.0f / 60, 1);
		require(std::abs(stopped.x) < 0.000001f, "Full drag failed to remove inertia");
		for (const auto shape : {avatar::SpringWorldCollider{Vector3(0.15f, 0.9f, 0), Vector3(0.15f, 0.9f, 0), 0.2f},
			avatar::SpringWorldCollider{Vector3(0.15f, 0.8f, 0), Vector3(0.15f, 1.0f, 0), 0.2f}})
		{
			const auto tail = avatar::collideSpringTail(head, Vector3(0.3f, 0.954f, 0), 1, 0.05f, shape, rest);
			const auto pointTail = avatar::collideSpringTail(head, Vector3(0.3f, 0.954f, 0), 1, 0, shape, rest);
			require(std::abs(tail.length() - 1) < 0.000001f, "Collision changed the bone length");
			require(tail.x > 0.3f, "Collision did not push the tail away from the collider");
			require(tail.x > pointTail.x, "Joint hit radius was omitted from collision");
		}
		const avatar::SpringWorldCollider collapsed = {current, current, 0.2f};
		const auto coincident = avatar::collideSpringTail(head, current, 1, 0.05f, collapsed, rest);
		require(std::isfinite(coincident.x) && std::abs(coincident.length() - 1) < 0.000001f, "Coincident collider produced an invalid tail");
		std::cout << "Spring inertia, zero time, drag, sphere, capsule and length tests passed\n";
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
