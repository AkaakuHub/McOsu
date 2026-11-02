#include "VrmGeometry.h"
#include "cgltf.h"
#include <stdexcept>

namespace avatar
{
static const cgltf_accessor *attribute(const cgltf_primitive &primitive, cgltf_attribute_type type)
{
	const auto *accessor = cgltf_find_accessor(&primitive, type, 0);
	if (!accessor) throw std::runtime_error("VRM primitive is missing a required vertex attribute");
	return accessor;
}

std::vector<Primitive> readGeometry(const cgltf_data &scene)
{
	std::vector<Primitive> output;
	for (std::size_t nodeIndex = 0; nodeIndex < scene.nodes_count; ++nodeIndex)
	{
		const auto &node = scene.nodes[nodeIndex];
		if (!node.mesh) continue;
		if (!node.skin) throw std::runtime_error("Avatar mesh has no skin");
		for (std::size_t index = 0; index < node.mesh->primitives_count; ++index)
		{
			const auto &source = node.mesh->primitives[index];
			if (source.type != cgltf_primitive_type_triangles || !source.indices || !source.material)
				throw std::runtime_error("Avatar primitives must be indexed triangles with a material");
			const auto *positions = attribute(source, cgltf_attribute_type_position);
			const auto *normals = attribute(source, cgltf_attribute_type_normal);
			const auto *uv = attribute(source, cgltf_attribute_type_texcoord);
			const auto *weights = attribute(source, cgltf_attribute_type_weights);
			const auto *joints = attribute(source, cgltf_attribute_type_joints);
			Primitive primitive;
			primitive.node = nodeIndex;
			primitive.material = static_cast<std::size_t>(source.material - scene.materials);
			primitive.vertices.resize(positions->count);
			for (std::size_t vertexIndex = 0; vertexIndex < positions->count; ++vertexIndex)
			{
				auto &vertex = primitive.vertices[vertexIndex];
				if (!cgltf_accessor_read_float(positions, vertexIndex, vertex.position.data(), 3)
					|| !cgltf_accessor_read_float(normals, vertexIndex, vertex.normal.data(), 3)
					|| !cgltf_accessor_read_float(uv, vertexIndex, vertex.uv.data(), 2)
					|| !cgltf_accessor_read_float(weights, vertexIndex, vertex.weights.data(), 4)
					|| !cgltf_accessor_read_uint(joints, vertexIndex, vertex.joints.data(), 4))
					throw std::runtime_error("Cannot decode avatar vertex data");
				for (auto joint : vertex.joints)
					if (joint >= node.skin->joints_count) throw std::runtime_error("Invalid skin joint index");
			}
			primitive.indices.reserve(source.indices->count);
			for (std::size_t vertexIndex = 0; vertexIndex < source.indices->count; ++vertexIndex)
			{
				const auto value = cgltf_accessor_read_index(source.indices, vertexIndex);
				if (value >= primitive.vertices.size()) throw std::runtime_error("Invalid avatar triangle index");
				primitive.indices.push_back(static_cast<unsigned int>(value));
			}
			output.push_back(std::move(primitive));
		}
	}
	return output;
}
}
