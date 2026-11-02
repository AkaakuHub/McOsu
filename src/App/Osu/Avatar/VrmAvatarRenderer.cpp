#include "VrmAvatarRenderer.h"
#include "VrmSkinning.h"
#include "cbase.h"
#include "Image.h"
#include "Shader.h"
#include "VertexArrayObject.h"
#include "cgltf.h"
#include <cstring>
#include <stdexcept>

namespace avatar
{
VrmAvatarRenderer::VrmAvatarRenderer(Graphics *graphics, const std::string &path)
	: m_document(path), m_geometry(readGeometry(m_document.scene()))
{
	const auto &scene = m_document.scene();
	for (std::size_t index = 0; index < scene.images_count; ++index)
	{
		const auto &source = scene.images[index];
		if (!source.buffer_view || !source.mime_type || std::strcmp(source.mime_type, "image/png") != 0)
			throw std::runtime_error("Avatar images must be embedded PNGs");
		const auto *bytes = cgltf_buffer_view_data(source.buffer_view);
		if (!bytes) throw std::runtime_error("Avatar image buffer is missing");
		std::unique_ptr<Image> image(graphics->createImage(1, 1, false, false));
		image->setPixels(reinterpret_cast<const char *>(bytes), source.buffer_view->size, Image::TYPE::TYPE_PNG);
		image->load();
		if (!image->isReady()) throw std::runtime_error("Avatar texture upload failed");
		m_images.push_back(std::move(image));
	}
	m_white.reset(graphics->createImage(1, 1, false, false));
	m_white->setPixel(0, 0, 0xffffffff);
	m_white->load();
}

VrmAvatarRenderer::~VrmAvatarRenderer() = default;

void VrmAvatarRenderer::draw(Graphics *graphics, Shader *shader, const Matrix4 &viewProjection)
{
	const auto &scene = m_document.scene();
	shader->enable();
	auto matrix = viewProjection;
	shader->setUniformMatrix4fv("matrix", matrix);
	for (const auto &primitive : m_geometry)
	{
		const auto &material = scene.materials[primitive.material];
		const auto &base = material.pbr_metallic_roughness;
		auto *image = m_white.get();
		if (base.base_color_texture.texture)
			image = m_images.at(base.base_color_texture.texture->image - scene.images).get();
		image->bind();
		const auto positions = skinPositions(scene, primitive);
		VertexArrayObject vertices;
		for (auto index : primitive.indices)
		{
			vertices.addVertex(positions[index]);
			vertices.addTexcoord(primitive.vertices[index].uv[0], primitive.vertices[index].uv[1]);
		}
		const auto &factor = base.base_color_factor;
		graphics->setColor((static_cast<unsigned int>(factor[3] * 255) << 24)
			| (static_cast<unsigned int>(factor[0] * 255) << 16)
			| (static_cast<unsigned int>(factor[1] * 255) << 8)
			| static_cast<unsigned int>(factor[2] * 255));
		graphics->drawVAO(&vertices);
		image->unbind();
	}
	shader->disable();
}
}
