#include "VrmAvatarRenderer.h"
#include "VrmSkinning.h"
#include "VrmTransforms.h"
#include "cbase.h"
#include "Engine.h"
#include "ConVar.h"
#include "Image.h"
#include "Shader.h"
#include "VertexArrayObject.h"
#include "cgltf.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <chrono>

ConVar osu_vr_avatar_waist_tracker("osu_vr_avatar_waist_tracker", "", FCVAR_NONE);
ConVar osu_vr_avatar_left_foot_tracker("osu_vr_avatar_left_foot_tracker", "", FCVAR_NONE);
ConVar osu_vr_avatar_right_foot_tracker("osu_vr_avatar_right_foot_tracker", "", FCVAR_NONE);
ConVar osu_vr_avatar_left_elbow_tracker("osu_vr_avatar_left_elbow_tracker", "", FCVAR_NONE);
ConVar osu_vr_avatar_right_elbow_tracker("osu_vr_avatar_right_elbow_tracker", "", FCVAR_NONE);

namespace avatar
{
VrmAvatarRenderer::VrmAvatarRenderer(Graphics *graphics, const std::string &path)
	: m_document(path), m_pose(m_document), m_springs(m_document), m_geometry(readGeometry(m_document.scene()))
{
	m_skinnedPositions.resize(m_geometry.size());
	for (auto *setting : {&osu_vr_avatar_waist_tracker, &osu_vr_avatar_left_foot_tracker,
		&osu_vr_avatar_right_foot_tracker, &osu_vr_avatar_left_elbow_tracker, &osu_vr_avatar_right_elbow_tracker})
	{
		const auto name = std::string("MCOSU_") + setting->getName().toUtf8();
		if (const auto *serial = std::getenv(name.c_str())) setting->setValue(UString(serial));
	}
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
	const auto started = std::chrono::steady_clock::now();
	auto frame = tracking;
	frame.deltaTime = static_cast<float>(engine->getFrameTime());
	const std::array<std::string, 5> bindings = {{osu_vr_avatar_waist_tracker.getString().toUtf8(),
		osu_vr_avatar_left_foot_tracker.getString().toUtf8(), osu_vr_avatar_right_foot_tracker.getString().toUtf8(),
		osu_vr_avatar_left_elbow_tracker.getString().toUtf8(), osu_vr_avatar_right_elbow_tracker.getString().toUtf8()}};
	if (bindings != m_trackerBindings)
	{
		m_trackerBindings = bindings;
		m_bodyTracking.setBindings(bindings);
		m_pose.recalibrate();
	}
	const auto body = m_bodyTracking.sample();
	std::vector<std::string> trackerDevices;
	for (const auto &device : m_bodyTracking.devices())
		trackerDevices.push_back(device.serial + ":" + device.controllerType);
	if (trackerDevices != m_trackerDevices)
	{
		m_trackerDevices = trackerDevices;
		for (const auto &device : m_bodyTracking.devices())
			debugLog("VRM tracker device: serial=%s controllerType=%s\n", device.serial.c_str(), device.controllerType.c_str());
	}
	frame.waist = body.waist;
	frame.leftFoot = body.leftFoot;
	frame.rightFoot = body.rightFoot;
	frame.leftElbow = body.leftElbow;
	frame.rightElbow = body.rightElbow;
	frame.hasTrackingSpaceDelta = body.hasTrackingSpaceDelta;
	frame.trackingSpaceDelta = body.trackingSpaceDelta;
	if (body.hasTrackingSpaceDelta) m_springs.reset();
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
		| (static_cast<int>(m_handTracking.status(HandSide::Right)) << 6)
		| (static_cast<int>(body.status[0]) << 9) | (static_cast<int>(body.status[1]) << 12)
		| (static_cast<int>(body.status[2]) << 15)
		| (static_cast<int>(body.status[3]) << 18) | (static_cast<int>(body.status[4]) << 21);
	if (mask != m_trackingMask)
	{
		debugLog("VRM tracking: inUse=%i head=%i palmLeft=%i palmRight=%i handStatus=(%i %i) headPosition=(%.3f %.3f %.3f)\n",
			frame.headsetInUse, frame.head.valid, frame.leftHand.valid, frame.rightHand.valid,
			static_cast<int>(m_handTracking.status(HandSide::Left)), static_cast<int>(m_handTracking.status(HandSide::Right)),
			tracking.head.matrix[12], tracking.head.matrix[13], tracking.head.matrix[14]);
		m_trackingMask = mask;
		debugLog("VRM body tracking: waist=%i leftFoot=%i rightFoot=%i leftElbow=%i rightElbow=%i status=(%i %i %i %i %i)\n",
			frame.waist.valid, frame.leftFoot.valid, frame.rightFoot.valid, frame.leftElbow.valid, frame.rightElbow.valid,
			static_cast<int>(body.status[0]), static_cast<int>(body.status[1]), static_cast<int>(body.status[2]),
			static_cast<int>(body.status[3]), static_cast<int>(body.status[4]));
	}
	m_pose.update(frame);
	const auto posed = std::chrono::steady_clock::now();
	if (frame.head.valid && frame.headsetInUse && engine->getTime() >= m_armLogTime)
	{
		m_armLogTime = engine->getTime() + 0.1;
		for (const std::string side : {"left", "right"})
		{
			const auto joint = [&](const char *name) {
				return point(m_pose.modelToTracking(), position(world(m_document.scene().nodes[m_document.humanBones().at(side + name)])));
			};
			const auto shoulder = joint("UpperArm"), elbow = joint("LowerArm"), wrist = joint("Hand");
			const auto upper = (elbow - shoulder).normalize(), lower = (wrist - elbow).normalize();
			const auto &input = side == "left" ? frame.leftHand : frame.rightHand;
			const auto &elbowInput = side == "left" ? frame.leftElbow : frame.rightElbow;
			debugLog("VRM arm: t=%.3f side=%s valid=%i shoulder=(%.4f %.4f %.4f) elbow=(%.4f %.4f %.4f) wrist=(%.4f %.4f %.4f) input=(%.4f %.4f %.4f) forward=(%.4f %.4f %.4f) bend=%.2f\n",
				engine->getTime(), side.c_str(), input.valid, shoulder.x, shoulder.y, shoulder.z,
				elbow.x, elbow.y, elbow.z, wrist.x, wrist.y, wrist.z,
				input.matrix[12], input.matrix[13], input.matrix[14], -input.matrix[8], -input.matrix[9], -input.matrix[10],
				std::acos(std::max(-1.0f, std::min(1.0f, upper.dot(lower)))) * 180.0f / 3.141592654f);
			if (elbowInput.valid)
				debugLog("VRM elbow tracker: side=%s serial=%s measured=(%.4f %.4f %.4f) solvedError=%.4f\n",
					side.c_str(), elbowInput.deviceId.c_str(), elbowInput.matrix[12], elbowInput.matrix[13], elbowInput.matrix[14],
					(elbow - position(elbowInput.matrix)).length());
		}
	}
	if (frame.head.valid && frame.headsetInUse)
		m_springs.update(static_cast<float>(engine->getFrameTime()), m_pose.modelToTracking());
	else m_springs.reset();
	const auto sprung = std::chrono::steady_clock::now();
	for (std::size_t index = 0; index < m_geometry.size(); ++index)
		m_skinnedPositions[index] = skinPositions(m_document.scene(), m_geometry[index]);
	const auto skinned = std::chrono::steady_clock::now();
	if (engine->getTime() >= m_performanceLogTime)
	{
		m_performanceLogTime = engine->getTime() + 1;
		debugLog("VRM frame: frameMs=%.2f poseMs=%.2f springMs=%.2f skinMs=%.2f\n",
			engine->getFrameTime() * 1000,
			std::chrono::duration<double, std::milli>(posed - started).count(),
			std::chrono::duration<double, std::milli>(sprung - posed).count(),
			std::chrono::duration<double, std::milli>(skinned - sprung).count());
	}
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
		const auto &positions = m_skinnedPositions[primitiveIndex];
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
