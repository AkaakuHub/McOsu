#include "VrmVisibility.h"
#include "VrmDocument.h"
#include "cgltf.h"
#include "json.hpp"
#include <map>
#include <stdexcept>

namespace avatar
{
std::vector<ViewIndices> readViewIndices(const VrmDocument &document, const std::vector<Primitive> &geometry)
{
	const auto &scene = document.scene();
	const auto json = nlohmann::json::parse(scene.json, scene.json + scene.json_size);
	const auto &vrm = json.at("extensions").at("VRMC_vrm");
	std::map<std::size_t, std::string> annotations;
	if (vrm.contains("firstPerson") && vrm.at("firstPerson").contains("meshAnnotations"))
		for (const auto &annotation : vrm.at("firstPerson").at("meshAnnotations"))
			annotations.emplace(annotation.at("node").get<std::size_t>(), annotation.at("type").get<std::string>());
	const auto *head = &scene.nodes[document.humanBones().at("head")];
	std::vector<ViewIndices> output;
	for (const auto &primitive : geometry)
	{
		const auto found = annotations.find(primitive.node);
		const auto flag = found == annotations.end() ? "auto" : found->second;
		if (flag != "auto" && flag != "both" && flag != "firstPersonOnly" && flag != "thirdPersonOnly")
			throw std::runtime_error("Invalid VRM first-person annotation");
		ViewIndices views;
		if (flag != "firstPersonOnly") views.thirdPerson = primitive.indices;
		if (flag == "both" || flag == "firstPersonOnly") views.firstPerson = primitive.indices;
		if (flag == "auto")
		{
			const auto *skin = scene.nodes[primitive.node].skin;
			std::vector<bool> headVertices(primitive.vertices.size(), false);
			for (std::size_t index = 0; index < primitive.vertices.size(); ++index)
				for (std::size_t influence = 0; influence < 4; ++influence)
				{
					const auto &vertex = primitive.vertices[index];
					if (vertex.weights[influence] == 0) continue;
					for (auto *node = skin->joints[vertex.joints[influence]]; node; node = node->parent)
						if (node == head) { headVertices[index] = true; break; }
				}
			for (std::size_t index = 0; index < primitive.indices.size(); index += 3)
			{
				if (headVertices[primitive.indices[index]] || headVertices[primitive.indices[index + 1]]
					|| headVertices[primitive.indices[index + 2]]) continue;
				views.firstPerson.insert(views.firstPerson.end(), primitive.indices.begin() + index, primitive.indices.begin() + index + 3);
			}
		}
		output.push_back(std::move(views));
	}
	return output;
}
}
