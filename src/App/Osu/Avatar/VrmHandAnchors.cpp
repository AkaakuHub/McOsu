#include "VrmHandAnchors.h"
#include "VrmDocument.h"
#include "VrmGeometry.h"
#include "VrmSkinning.h"
#include "VrmTransforms.h"
#include "cgltf.h"
#include <limits>
#include <cmath>
#include <stdexcept>

namespace avatar
{
namespace
{
float intersection(Vector3 origin, Vector3 direction, Vector3 a, Vector3 b, Vector3 c)
{
	const auto edge1 = b - a;
	const auto edge2 = c - a;
	const auto cross = direction.cross(edge2);
	const float determinant = edge1.dot(cross);
	if (std::abs(determinant) < 0.00000001f) return std::numeric_limits<float>::infinity();
	const auto offset = origin - a;
	const float u = offset.dot(cross) / determinant;
	const auto q = offset.cross(edge1);
	const float v = direction.dot(q) / determinant;
	const float distance = edge2.dot(q) / determinant;
	return u >= 0 && v >= 0 && u + v <= 1 && distance >= 0
		? distance : std::numeric_limits<float>::infinity();
}
}

std::array<Vector3, 2> measurePalmAnchors(const VrmDocument &document)
{
	const auto &scene = document.scene();
	const auto geometry = readGeometry(scene);
	std::array<Vector3, 2> anchors;
	std::size_t sideIndex = 0;
	for (const std::string side : {"left", "right"})
	{
		const auto node = [&document, &scene](const std::string &name) -> const cgltf_node & {
			return scene.nodes[document.humanBones().at(name)];
		};
		const auto &hand = node(side + "Hand");
		const auto wrist = position(world(hand));
		const auto middle = position(world(node(side + "MiddleProximal")));
		const auto index = position(world(node(side + "IndexProximal")));
		const auto little = position(world(node(side + "LittleProximal")));
		auto up = (index - wrist).cross(little - wrist).normalize();
		if (up.y < 0) up = -up;
		const auto frame = handFrame(middle - wrist, up);
		const auto center = (wrist + (index + middle + little) / 3) * 0.5f;
		const float handLength = (middle - wrist).length();
		const auto rayOrigin = center - up * (handLength * 2);
		float nearest = std::numeric_limits<float>::infinity();
		for (const auto &primitive : geometry)
		{
			const auto *skin = scene.nodes[primitive.node].skin;
			std::vector<bool> handVertices(primitive.vertices.size(), false);
			for (std::size_t vertex = 0; vertex < primitive.vertices.size(); ++vertex)
				for (std::size_t influence = 0; influence < 4; ++influence)
				{
					if (primitive.vertices[vertex].weights[influence] == 0) continue;
					for (auto *joint = skin->joints[primitive.vertices[vertex].joints[influence]]; joint; joint = joint->parent)
						if (joint == &hand) { handVertices[vertex] = true; break; }
				}
			const auto vertices = skinPositions(scene, primitive);
			for (std::size_t triangle = 0; triangle < primitive.indices.size(); triangle += 3)
			{
				const auto a = primitive.indices[triangle], b = primitive.indices[triangle + 1], c = primitive.indices[triangle + 2];
				if (!handVertices[a] || !handVertices[b] || !handVertices[c]) continue;
				nearest = std::min(nearest, intersection(rayOrigin, up, vertices[a], vertices[b], vertices[c]));
			}
		}
		if (!std::isfinite(nearest) || nearest > handLength * 4)
			throw std::runtime_error("Cannot measure the avatar's " + side + " palm surface");
		anchors[sideIndex++] = Matrix4(frame).invert() * (rayOrigin + up * nearest - wrist);
	}
	return anchors;
}
}
