#ifndef MCOSU_FOOT_LOCOMOTION_H
#define MCOSU_FOOT_LOCOMOTION_H

#include "Matrices.h"
#include <array>
#include <deque>

namespace avatar
{
class FootLocomotion
{
public:
	struct Config
	{
		std::array<Matrix4, 2> restFeet;
		Vector3 restPelvis;
		std::array<float, 2> legLength = {{1, 1}};
	};
	struct TrackedFoot { bool valid = false; Matrix4 pose; };
	struct Input
	{
		float dt = 0;
		Matrix4 modelToTracking;
		Matrix4 head;
		std::array<Vector3, 2> hands;
		Vector3 hipsWorld;
		Vector3 balancePoint;
		std::array<TrackedFoot, 2> trackedFeet;
		bool pelvisTracked = false;
		bool hasTrackingSpaceDelta = false;
		Matrix4 trackingSpaceDelta;
	};
	struct Foot { Matrix4 pose; bool tracked = false; bool stepping = false; };
	void configure(const Config &config);
	void reset();
	const std::array<Foot, 2> &update(const Input &input);

private:
	struct Sample { double time; Vector3 position; float yaw; Vector3 velocity; float angularVelocity; float duration; };
	struct Step { Matrix4 start; Matrix4 end; float elapsed = 0; float duration = 0.22f; float height = 0; };
	void rebase(const Matrix4 &delta);
	Config m_config;
	std::array<Foot, 2> m_feet;
	std::array<Step, 2> m_steps;
	std::array<bool, 2> m_recovering = {{false, false}};
	std::deque<Sample> m_history;
	double m_time = 0;
	float m_motionWeight = 0;
	float m_floorY = 0;
	bool m_initialized = false;
	int m_lastStep = 1;
};
}
#endif
