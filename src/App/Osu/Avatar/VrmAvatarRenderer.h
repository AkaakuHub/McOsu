#ifndef MCOSU_VRM_AVATAR_RENDERER_H
#define MCOSU_VRM_AVATAR_RENDERER_H

#include "VrmDocument.h"
#include "VrmGeometry.h"
#include "VrmPose.h"
#include "VrmVisibility.h"
#include "OpenVRHandTracking.h"
#include "OpenVRBodyTracking.h"
#include "Spring/SpringRuntime.h"
#include <memory>

class Graphics;
class Image;
class Shader;
class Matrix4;

namespace avatar
{
class VrmAvatarRenderer
{
public:
	VrmAvatarRenderer(Graphics *graphics, const std::string &path);
	~VrmAvatarRenderer();
	void draw(Graphics *graphics, const Matrix4 &viewProjection, bool firstPerson);
	void updateTracking(const TrackingFrame &tracking);
	void recalibrate() { m_pose.recalibrate(); m_springs.reset(); }
	const Matrix4 &subjectTransform() const { return m_pose.modelToTracking(); }
	float subjectHeight() const { return m_subjectHeight; }
	bool hasCalibration() const { return m_pose.hasCalibration(); }
	float floorHeight() const { return m_pose.modelToTracking()[13]; }

private:
	VrmDocument m_document;
	VrmPose m_pose;
	SpringRuntime m_springs;
	OpenVRHandTracking m_handTracking;
	OpenVRBodyTracking m_bodyTracking;
	std::array<std::string, 5> m_trackerBindings;
	std::vector<std::string> m_trackerDevices;
	std::vector<Primitive> m_geometry;
	std::vector<ViewIndices> m_viewIndices;
	std::vector<std::unique_ptr<Image>> m_images;
	std::unique_ptr<Image> m_white;
	std::unique_ptr<Shader> m_shader;
	int m_trackingMask = -1;
	float m_subjectHeight = 0;
	double m_poseFrameTime = -1;
	double m_armLogTime = 0;
};
}

#endif
