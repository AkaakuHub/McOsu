#include "VrmAvatarRenderer.h"
#include "VrmSkinning.h"
#include "cbase.h"
#include "Engine.h"
#include "Image.h"
#include "Shader.h"
#include "VertexArrayObject.h"
#include "cgltf.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace avatar
{
VrmAvatarRenderer::VrmAvatarRenderer(Graphics *graphics, const std::string &path)
	: m_document(path), m_pose(m_document), m_springs(m_document), m_geometry(readGeometry(m_document.scene()))
{
	m_shader.reset(graphics->createShaderFromSource(
		"#version 110\nuniform mat4 matrix; varying vec2 uv; void main(){uv=gl_MultiTexCoord0.xy; gl_Position=matrix*gl_Vertex; gl_FrontColor=gl_Color;}",
		"#version 110\nuniform sampler2D mytexture; uniform int alphaMode; uniform float alphaCutoff; uniform int cullBackfaces; varying vec2 uv; void main(){if(cullBackfaces!=0 && !gl_FrontFacing) discard; vec4 color=texture2D(mytexture,uv)*gl_Color; if(alphaMode==1 && color.a<alphaCutoff) discard; if(alphaMode!=2) color.a=1.0; gl_FragColor=color;}"));
	m_shader->load();
	if (!m_shader->isReady()) throw std::runtime_error("Avatar shader compilation failed");
	const auto &scene = m_document.scene();
	std::stable_sort(m_geometry.begin(), m_geometry.end(), [&scene](const Primitive &left, const Primitive &right) {
		return (scene.materials[left.material].alpha_mode == cgltf_alpha_mode_blend)
			< (scene.materials[right.material].alpha_mode == cgltf_alpha_mode_blend);
	});
	m_viewIndices = readViewIndices(m_document, m_geometry);
	float bottom = 0, top = 0;
	for (const auto &primitive : m_geometry)
		for (const auto &vertex : skinPositions(scene, primitive))
		{
			bottom = std::min(bottom, vertex.y);
			top = std::max(top, vertex.y);
		}
	m_subjectHeight = top - bottom;
	for (std::size_t index = 0; index < scene.images_count; ++index)
	{
		const auto &source = scene.images[index];
		if (!source.buffer_view || !source.mime_type || std::strcmp(source.mime_type, "image/png") != 0)
			throw std::runtime_error("Invalid avatar image " + std::to_string(index)
				+ ": mime=" + (source.mime_type ? source.mime_type : "missing")
				+ ", buffer=" + (source.buffer_view ? "present" : "missing"));
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

void VrmAvatarRenderer::updateTracking(const TrackingFrame &tracking)
{
	if (m_poseFrameTime == engine->getTime()) return;
	m_poseFrameTime = engine->getTime();
	auto frame = tracking;
	frame.head.valid = frame.head.valid && frame.headsetInUse;
	for (const auto side : {HandSide::Left, HandSide::Right})
	{
		auto &hand = side == HandSide::Left ? frame.leftHand : frame.rightHand;
		if (!frame.headsetInUse) hand.valid = false;
		if (!hand.valid) continue;
		Matrix4 palm;
		hand.valid = m_handTracking.tryGetPalmPose(side, hand.matrix, palm);
		if (hand.valid)
		{
			hand.matrix = palm;
			hand.palmSurface = true;
		}
	}
	const int mask = (frame.head.valid ? 1 : 0) | (frame.leftHand.valid ? 2 : 0) | (frame.rightHand.valid ? 4 : 0)
		| (static_cast<int>(m_handTracking.status(HandSide::Left)) << 3)
		| (static_cast<int>(m_handTracking.status(HandSide::Right)) << 6);
	if (mask != m_trackingMask)
	{
		debugLog("VRM tracking: inUse=%i head=%i palmLeft=%i palmRight=%i handStatus=(%i %i) headPosition=(%.3f %.3f %.3f)\n",
			frame.headsetInUse, frame.head.valid, frame.leftHand.valid, frame.rightHand.valid,
			static_cast<int>(m_handTracking.status(HandSide::Left)), static_cast<int>(m_handTracking.status(HandSide::Right)),
			tracking.head.matrix[12], tracking.head.matrix[13], tracking.head.matrix[14]);
		m_trackingMask = mask;
	}
	m_pose.update(frame);
	if (frame.head.valid && frame.headsetInUse)
		m_springs.update(static_cast<float>(engine->getFrameTime()), m_pose.modelToTracking());
	else m_springs.reset();
}

void VrmAvatarRenderer::draw(Graphics *graphics, const Matrix4 &viewProjection, bool firstPerson)
{
	const auto &scene = m_document.scene();
	auto *shader = m_shader.get();
	shader->enable();
	auto matrix = viewProjection * m_pose.modelToTracking();
	shader->setUniformMatrix4fv("matrix", matrix);
	for (std::size_t primitiveIndex = 0; primitiveIndex < m_geometry.size(); ++primitiveIndex)
	{
		const auto &primitive = m_geometry[primitiveIndex];
		const auto &views = m_viewIndices[primitiveIndex];
		const auto &indices = firstPerson ? views.firstPerson : views.thirdPerson;
		if (indices.empty()) continue;
		const auto &material = scene.materials[primitive.material];
		shader->setUniform1i("cullBackfaces", firstPerson || !material.double_sided ? 1 : 0);
		shader->setUniform1i("alphaMode", static_cast<int>(material.alpha_mode));
		shader->setUniform1f("alphaCutoff", material.alpha_cutoff);
		const auto &base = material.pbr_metallic_roughness;
		auto *image = m_white.get();
		if (base.base_color_texture.texture)
			image = m_images.at(base.base_color_texture.texture->image - scene.images).get();
		image->bind();
		const auto positions = skinPositions(scene, primitive);
		VertexArrayObject vertices;
		for (auto index : indices)
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
