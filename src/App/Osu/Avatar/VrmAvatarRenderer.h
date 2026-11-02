#ifndef MCOSU_VRM_AVATAR_RENDERER_H
#define MCOSU_VRM_AVATAR_RENDERER_H

#include "VrmDocument.h"
#include "VrmGeometry.h"
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

private:
	VrmDocument m_document;
	std::vector<Primitive> m_geometry;
	std::vector<std::unique_ptr<Image>> m_images;
	std::unique_ptr<Image> m_white;
	std::unique_ptr<Shader> m_shader;
};
}

#endif
