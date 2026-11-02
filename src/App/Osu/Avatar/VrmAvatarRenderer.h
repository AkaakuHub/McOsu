#ifndef MCOSU_VRM_AVATAR_RENDERER_H
#define MCOSU_VRM_AVATAR_RENDERER_H

#include "VrmDocument.h"
#include "VrmGeometry.h"
#include "VrmPose.h"
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
	void draw(Graphics *graphics, const Matrix4 &viewProjection);
	void updateTracking(const TrackingFrame &tracking);
	void recalibrate() { m_pose.recalibrate(); }

private:
	VrmDocument m_document;
	VrmPose m_pose;
	std::vector<Primitive> m_geometry;
	std::vector<std::unique_ptr<Image>> m_images;
	std::unique_ptr<Image> m_white;
	std::unique_ptr<Shader> m_shader;
	int m_trackingMask = -1;
};
}

#endif
