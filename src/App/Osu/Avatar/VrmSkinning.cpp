#include "VrmSkinning.h"
#include "Matrices.h"
#include "cgltf.h"
#include <cmath>
#include <stdexcept>

namespace avatar
{
std::vector<Vector3> skinPositions(const cgltf_data &scene, const Primitive &primitive)
{
	if (primitive.node >= scene.nodes_count) throw std::runtime_error("Missing mesh node");
	const auto *skin = scene.nodes[primitive.node].skin;
	if (!skin || !skin->inverse_bind_matrices) throw std::runtime_error("Missing avatar bind matrices");
	std::vector<Matrix4> matrices;
	matrices.reserve(skin->joints_count);
	for (std::size_t index = 0; index < skin->joints_count; ++index)
	{
		float world[16], inverseBind[16];
		cgltf_node_transform_world(skin->joints[index], world);
		if (!cgltf_accessor_read_float(skin->inverse_bind_matrices, index, inverseBind, 16))
			throw std::runtime_error("Cannot decode inverse bind matrix");
		matrices.push_back(Matrix4(world) * Matrix4(inverseBind));
	}
	std::vector<Vector3> output;
	output.reserve(primitive.vertices.size());
	for (const auto &vertex : primitive.vertices)
	{
		Vector3 position(0, 0, 0);
		float total = 0;
		for (std::size_t influence = 0; influence < 4; ++influence)
		{
			const auto weight = vertex.weights[influence];
			if (!std::isfinite(weight) || weight < 0) throw std::runtime_error("Invalid skin weight");
			if (weight == 0) continue;
			if (vertex.joints[influence] >= matrices.size()) throw std::runtime_error("Missing skin joint");
			const auto transformed = matrices[vertex.joints[influence]] * Vector4(
				vertex.position[0], vertex.position[1], vertex.position[2], 1);
			position += Vector3(transformed.x, transformed.y, transformed.z) * weight;
			total += weight;
		}
		if (std::abs(total - 1) > 0.001f) throw std::runtime_error("Skin weights are not normalized");
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
			throw std::runtime_error("Skinning produced non-finite positions");
		output.push_back(position);
	}
	return output;
}
}
