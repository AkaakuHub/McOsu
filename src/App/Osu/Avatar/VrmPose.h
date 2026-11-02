#ifndef MCOSU_VRM_POSE_H
#define MCOSU_VRM_POSE_H

#include "Matrices.h"
#include <vector>
#include <string>
#include <chrono>
#include <array>

struct cgltf_node;

namespace avatar
{
class VrmDocument;
struct TrackedPose { bool valid = false; Matrix4 matrix; bool palmSurface = false; };
struct TrackingFrame { TrackedPose head; TrackedPose leftHand; TrackedPose rightHand; bool headsetInUse = true; };

class VrmPose
{
public:
	explicit VrmPose(VrmDocument &document);
	void update(const TrackingFrame &tracking);
	void recalibrate();
	const Matrix4 &modelToTracking() const { return m_modelToTracking; }
	bool hasCalibration() const { return m_scale > 0; }

private:
	cgltf_node &bone(const std::string &name);
	void moveLimb(const std::string &side, bool arm, Vector3 target, Vector3 bend);
	void moveArm(const std::string &side, const Matrix4 &target, bool palmSurface);
	VrmDocument &m_document;
	std::vector<Matrix4> m_restLocal;
	std::vector<Matrix4> m_restWorld;
	Vector3 m_restEyes;
	bool m_calibrated = false;
	float m_scale = 0;
	float m_bodyYaw = 0;
	Vector3 m_previousHead;
	std::chrono::steady_clock::time_point m_lastUpdate;
	std::array<float, 2> m_armTwist = {{0, 0}};
	std::array<bool, 2> m_hasArmTwist = {{false, false}};
	std::array<Vector3, 2> m_palmAnchors;
	Matrix4 m_modelToTracking;
};
}

#endif
