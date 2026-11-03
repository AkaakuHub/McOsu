#ifndef MCOSU_VRM_POSE_H
#define MCOSU_VRM_POSE_H

#include "Matrices.h"
#include <vector>
#include <string>
#include "TrackingTypes.h"
#include "Locomotion/FootLocomotion.h"
#include <array>
#include <deque>

struct cgltf_node;

namespace avatar
{
class VrmDocument;

class VrmPose
{
public:
	explicit VrmPose(VrmDocument &document);
	void update(const TrackingFrame &tracking);
	void recalibrate();
	const Matrix4 &modelToTracking() const { return m_modelToTracking; }
	bool hasCalibration() const { return m_scale > 0; }
	Vector3 viewpoint() const;

private:
	cgltf_node &bone(const std::string &name);
	void moveLimb(const std::string &side, bool arm, Vector3 target, Vector3 bend);
	void moveArm(const std::string &side, const Matrix4 &target, bool palmSurface, const TrackedPose &elbow);
	VrmDocument &m_document;
	std::vector<Matrix4> m_restLocal;
	std::vector<Matrix4> m_restWorld;
	Vector3 m_restEyes;
	bool m_calibrated = false;
	float m_scale = 0;
	float m_trackingFloor = 0;
	Matrix4 m_filteredWaist;
	double m_lastWaistTime = -1;
	float m_bodyYaw = 0;
	Vector3 m_previousHead;
	struct TrackerBinding { bool calibrated = false; std::string deviceId; Matrix4 offset; };
	std::array<TrackerBinding, 3> m_trackerBindings;
	std::array<Matrix4, 3> m_previousBonePoses;
	bool m_hasPreviousBonePoses = false;
	FootLocomotion m_locomotion;
	std::array<FootLocomotion::Foot, 2> m_feet;
	bool m_hasFeet = false;
	bool m_hasPendingSpaceDelta = false;
	Matrix4 m_pendingSpaceDelta;
	void solveTorso(const Matrix4 &headPose, Vector3 eyeTarget, bool fixedPelvis);
	Matrix4 trackedBonePose(const TrackedPose &tracked, int index, const std::string &name); 
	std::array<float, 2> m_armTwist = {{0, 0}};
	std::array<bool, 2> m_hasArmTwist = {{false, false}};
	std::array<Vector3, 2> m_palmAnchors;
	Matrix4 m_modelToTracking;
	struct StablePose { double time; Matrix4 model; std::vector<Matrix4> nodes; };
	std::deque<StablePose> m_stablePoses;
	double m_trackingTime = 0;
	void rememberStablePose(const TrackingFrame &tracking, float speed);
	void freezeStablePose();
};
}

#endif

