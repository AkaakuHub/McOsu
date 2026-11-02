#ifndef MCOSU_VRM_GEOMETRY_H
#define MCOSU_VRM_GEOMETRY_H

#include <array>
#include <cstddef>
#include <vector>

struct cgltf_data;

namespace avatar
{
struct Vertex
{
	std::array<float, 3> position;
	std::array<float, 3> normal;
	std::array<float, 2> uv;
	std::array<float, 4> weights;
	std::array<unsigned int, 4> joints;
};

struct Primitive
{
	std::size_t node;
	std::size_t material;
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
};

std::vector<Primitive> readGeometry(const cgltf_data &scene);
}

#endif
