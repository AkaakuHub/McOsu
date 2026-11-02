#ifndef MCOSU_VRM_DOCUMENT_H
#define MCOSU_VRM_DOCUMENT_H

#include <cstddef>
#include <map>
#include <memory>
#include <string>

struct cgltf_data;

namespace avatar
{
class VrmDocument
{
public:
	explicit VrmDocument(const std::string &path);
	~VrmDocument();
	const cgltf_data &scene() const;
	const std::map<std::string, std::size_t> &humanBones() const;

private:
	struct Deleter { void operator()(cgltf_data *data) const; };
	std::unique_ptr<cgltf_data, Deleter> m_data;
	std::map<std::string, std::size_t> m_humanBones;
};
}

#endif
