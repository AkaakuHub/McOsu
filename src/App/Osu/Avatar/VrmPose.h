#ifndef MCOSU_VRM_POSE_H
#define MCOSU_VRM_POSE_H

#include "Matrices.h"
#include <vector>
#include <string>

struct cgltf_node;

namespace avatar
{
class VrmDocument;
struct TrackedPose { bool valid = false; Matrix4 matrix; };
struct TrackingFrame { TrackedPose head; TrackedPose leftHand; TrackedPose rightHand; };

class VrmPose
{
public:
	explicit VrmPose(VrmDocument &document);
	void update(const TrackingFrame &tracking);
	void recalibrate();
	const Matrix4 &modelToTracking() const { return m_modelToTracking; }

private:
	cgltf_node &bone(const std::string &name);
	void moveLimb(const std::string &side, bool arm, Vector3 target);
	VrmDocument &m_document;
	std::vector<Matrix4> m_restLocal;
	std::vector<Matrix4> m_restWorld;
	Vector3 m_restEyes;
	float m_scale = 0;
	Matrix4 m_modelToTracking;
};
}

#endif
