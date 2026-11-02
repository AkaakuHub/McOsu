#include "SpringDefinition.h"
#include "cgltf.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace avatar
{
namespace
{
std::size_t nodeIndex(const nlohmann::json &value, const cgltf_data &scene)
{
	const auto index = value.get<std::size_t>();
	if (index >= scene.nodes_count) throw std::runtime_error("SpringBone references a missing node");
	return index;
}
Vector3 vector(const nlohmann::json &value)
{
	if (!value.is_array() || value.size() != 3) throw std::runtime_error("SpringBone vector must have three components");
	Vector3 result(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
	if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z))
		throw std::runtime_error("SpringBone vector must be finite");
	return result;
}
float nonnegative(float value)
{
	if (!std::isfinite(value) || value < 0) throw std::runtime_error("SpringBone coefficient must be finite and nonnegative");
	return value;
}
int depth(const cgltf_node &node)
{
	int result = 0;
	for (auto *parent = node.parent; parent; parent = parent->parent) ++result;
	return result;
}
bool ancestor(const cgltf_node *parent, const cgltf_node *child)
{
	for (auto *node = child; node; node = node->parent) if (node == parent) return true;
	return false;
}
}

SpringDefinition readSpringDefinition(const cgltf_data &scene)
{
	SpringDefinition result;
	const auto document = nlohmann::json::parse(scene.json, scene.json + scene.json_size);
	const auto &extensions = document.at("extensions");
	if (!extensions.contains("VRMC_springBone")) return result;
	const auto &spring = extensions.at("VRMC_springBone");
	if (spring.at("specVersion") != "1.0") throw std::runtime_error("Only SpringBone 1.0 is supported");
	if (spring.contains("colliders")) for (const auto &source : spring.at("colliders"))
	{
		SpringCollider collider;
		collider.node = nodeIndex(source.at("node"), scene);
		const auto &shape = source.at("shape");
		if (shape.contains("sphere") == shape.contains("capsule")) throw std::runtime_error("Spring collider requires exactly one shape");
		const bool capsule = shape.contains("capsule");
		const auto &data = shape.at(capsule ? "capsule" : "sphere");
		collider.offset = data.contains("offset") ? vector(data.at("offset")) : Vector3();
		collider.tail = capsule ? vector(data.at("tail")) : collider.offset;
		collider.radius = nonnegative(data.value("radius", 0.0f));
		result.colliders.push_back(collider);
	}
	std::vector<std::vector<std::size_t>> groups;
	if (spring.contains("colliderGroups")) for (const auto &group : spring.at("colliderGroups"))
	{
		std::vector<std::size_t> indices;
		for (const auto &index : group.at("colliders"))
		{
			const auto value = index.get<std::size_t>();
			if (value >= result.colliders.size()) throw std::runtime_error("Spring collider group references a missing collider");
			indices.push_back(value);
		}
		groups.push_back(indices);
	}
	std::set<const cgltf_node *> claimed;
	if (spring.contains("springs")) for (const auto &source : spring.at("springs"))
	{
		SpringChain chain;
		if (source.contains("center")) chain.center = static_cast<int>(nodeIndex(source.at("center"), scene));
		for (const auto &joint : source.at("joints"))
		{
			SpringJointSettings settings;
			settings.node = nodeIndex(joint.at("node"), scene);
			settings.radius = nonnegative(joint.value("hitRadius", 0.0f));
			settings.stiffness = nonnegative(joint.value("stiffness", 1.0f));
			settings.gravity = nonnegative(joint.value("gravityPower", 0.0f));
			settings.drag = nonnegative(joint.value("dragForce", 0.5f));
			if (settings.drag > 1) throw std::runtime_error("SpringBone drag must be at most one");
			if (joint.contains("gravityDir")) settings.gravityDirection = vector(joint.at("gravityDir"));
			if (!chain.joints.empty() && (chain.joints.back().node == settings.node
				|| !ancestor(&scene.nodes[chain.joints.back().node], &scene.nodes[settings.node])))
				throw std::runtime_error("Spring joints must follow their ancestor order");
			const auto *stop = chain.joints.empty() ? nullptr : &scene.nodes[chain.joints.back().node];
			for (auto *node = &scene.nodes[settings.node]; node && node != stop; node = node->parent)
			{
				if (!claimed.insert(node).second) throw std::runtime_error("Spring chains contain duplicate or skipped joints");
				if (chain.joints.empty()) break;
			}
			chain.joints.push_back(settings);
		}
		if (chain.joints.empty()) throw std::runtime_error("Spring chain requires a joint");
		if (chain.center >= 0 && !ancestor(&scene.nodes[chain.center], &scene.nodes[chain.joints.front().node]))
			throw std::runtime_error("Spring center must be an ancestor of its first joint");
		if (source.contains("colliderGroups")) for (const auto &index : source.at("colliderGroups"))
		{
			const auto value = index.get<std::size_t>();
			if (value >= groups.size()) throw std::runtime_error("Spring references a missing collider group");
			chain.colliders.insert(chain.colliders.end(), groups[value].begin(), groups[value].end());
		}
		std::sort(chain.colliders.begin(), chain.colliders.end());
		chain.colliders.erase(std::unique(chain.colliders.begin(), chain.colliders.end()), chain.colliders.end());
		result.chains.push_back(chain);
	}
	for (std::size_t index = 0; index < result.chains.size(); ++index)
		if (result.chains[index].center >= 0)
			for (std::size_t other = 0; other < result.chains.size(); ++other) if (other != index)
				for (const auto &joint : result.chains[other].joints)
					if (ancestor(&scene.nodes[joint.node], &scene.nodes[result.chains[index].center]))
						throw std::runtime_error("Spring center cannot depend on another spring chain");
	std::stable_sort(result.chains.begin(), result.chains.end(), [&scene](const SpringChain &left, const SpringChain &right) {
		return depth(scene.nodes[left.joints.front().node]) < depth(scene.nodes[right.joints.front().node]);
	});
	return result;
}
}
