#include "VrmDocument.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "json.hpp"
#include <stdexcept>

namespace avatar
{
void VrmDocument::Deleter::operator()(cgltf_data *data) const
{
	cgltf_free(data);
}

VrmDocument::VrmDocument(const std::string &path)
{
	cgltf_options options = {};
	cgltf_data *parsed = nullptr;
	if (cgltf_parse_file(&options, path.c_str(), &parsed) != cgltf_result_success)
		throw std::runtime_error("Cannot parse VRM: " + path);
	m_data.reset(parsed);
	if (parsed->file_type != cgltf_file_type_glb)
		throw std::runtime_error("VRM must use the GLB container");
	if (cgltf_load_buffers(&options, parsed, path.c_str()) != cgltf_result_success
		|| cgltf_validate(parsed) != cgltf_result_success)
		throw std::runtime_error("Invalid VRM geometry or buffers");
	const auto document = nlohmann::json::parse(parsed->json, parsed->json + parsed->json_size);
	const auto &vrm = document.at("extensions").at("VRMC_vrm");
	if (vrm.at("specVersion").get<std::string>() != "1.0")
		throw std::runtime_error("Only VRM 1.0 is supported");
	for (const auto &bone : vrm.at("humanoid").at("humanBones").items())
	{
		const auto index = bone.value().at("node").get<std::size_t>();
		if (index >= parsed->nodes_count)
			throw std::runtime_error("Humanoid bone references a missing node");
		m_humanBones.emplace(bone.key(), index);
	}
}

VrmDocument::~VrmDocument() = default;

const cgltf_data &VrmDocument::scene() const { return *m_data; }
const std::map<std::string, std::size_t> &VrmDocument::humanBones() const { return m_humanBones; }
}
