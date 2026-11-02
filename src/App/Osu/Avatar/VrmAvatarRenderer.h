#ifndef MCOSU_VRM_AVATAR_RENDERER_H
#define MCOSU_VRM_AVATAR_RENDERER_H

#include "VrmDocument.h"
#include "VrmGeometry.h"
#include "VrmPose.h"
#include "VrmVisibility.h"
#include "OpenVRHandTracking.h"
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
	void recalibrate() { m_pose.recalibrate(); }
	const Matrix4 &subjectTransform() const { return m_pose.modelToTracking(); }
	float subjectHeight() const { return m_subjectHeight; }

private:
	VrmDocument m_document;
	VrmPose m_pose;
	OpenVRHandTracking m_handTracking;
	std::vector<Primitive> m_geometry;
	std::vector<ViewIndices> m_viewIndices;
	std::vector<std::unique_ptr<Image>> m_images;
	std::unique_ptr<Image> m_white;
	std::unique_ptr<Shader> m_shader;
	int m_trackingMask = -1;
	float m_subjectHeight = 0;
	double m_poseFrameTime = -1;
};
}

#endif
