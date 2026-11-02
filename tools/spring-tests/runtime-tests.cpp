#include "Spring/SpringRuntime.h"
#include "VrmDocument.h"
#include "VrmTransforms.h"
#include "cgltf.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }

struct Fixture
{
	std::string path;
	bool created = false;
	explicit Fixture(std::string value) : path(value) {}
	~Fixture() { if (created) std::remove(path.c_str()); }
	void write(bool center, bool gravity, bool collision)
	{
		if (!created && std::ifstream(path).good()) throw std::runtime_error("Synthetic fixture path already exists");
		const std::string json = std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],")
			+ "\"nodes\":[{\"children\":[1]},{\"children\":[2],\"translation\":[0,1,0]},{\"translation\":[0,1,0]}],"
			+ "\"extensions\":{\"VRMC_vrm\":{\"specVersion\":\"1.0\",\"humanoid\":{\"humanBones\":{\"head\":{\"node\":1}}}},"
			+ "\"VRMC_springBone\":{\"specVersion\":\"1.0\",\"colliders\":[{\"node\":0,\"shape\":{\"sphere\":{\"offset\":[0.15,1.9,0],\"radius\":0.2}}}],"
			+ "\"colliderGroups\":[{\"colliders\":[0]}],\"springs\":[{"
			+ (center ? "\"center\":0," : "") + (collision ? "\"colliderGroups\":[0]," : "")
			+ "\"joints\":[{\"node\":1,\"hitRadius\":0.05,\"stiffness\":1,\"dragForce\":0.4,\"gravityDir\":[1,0,0],\"gravityPower\":"
			+ (gravity ? "1" : "0") + "},{\"node\":2}]}]}}}";
		std::string padded = json;
		while (padded.size() % 4) padded += ' ';
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output) throw std::runtime_error("Cannot create synthetic SpringBone fixture");
		created = true;
		const auto word = [&output](unsigned int value) {
			for (int byte = 0; byte < 4; ++byte) output.put(static_cast<char>((value >> (byte * 8)) & 255));
		};
		word(0x46546c67); word(2); word(static_cast<unsigned int>(20 + padded.size()));
		word(static_cast<unsigned int>(padded.size())); word(0x4e4f534a);
		output.write(padded.data(), padded.size());
		if (!output) throw std::runtime_error("Cannot write synthetic SpringBone fixture");
	}
};

void restore(avatar::VrmDocument &model)
{
	for (std::size_t index = 0; index < model.scene().nodes_count; ++index) model.scene().nodes[index].has_matrix = false;
}
Vector3 tail(avatar::VrmDocument &model) { return avatar::position(avatar::world(model.scene().nodes[2])); }

Vector3 simulate(Fixture &fixture, float scale)
{
	avatar::VrmDocument model(fixture.path);
	avatar::SpringRuntime runtime(model);
	Matrix4 root;
	root.scale(scale);
	for (int frame = 0; frame < 60; ++frame)
	{
		restore(model);
		runtime.update(1.0f / 60, root);
	}
	const auto before = tail(model);
	for (int frame = 0; frame < 5; ++frame)
	{
		restore(model);
		runtime.update(0, root);
		require((tail(model) - before).length() < 0.00001f, "Zero time advanced the spring state");
	}
	restore(model);
	runtime.update(1, root);
	require((tail(model) - Vector3(0, 2, 0)).length() < 0.00001f, "Suspension did not reset history");
	return before;
}
}

int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	try
	{
		Fixture fixture(argv[1]);
		fixture.write(true, false, false);
		avatar::VrmDocument centered(fixture.path);
		avatar::SpringRuntime centeredRuntime(centered);
		bool rejected = false;
		try { centeredRuntime.update(-0.1f); } catch (const std::runtime_error &) { rejected = true; }
		require(rejected, "Negative delta time was accepted");
		centeredRuntime.update(1.0f / 60);
		Matrix4 moved;
		moved.rotateZ(30);
		moved.translate(0.25f, 0.1f, 0);
		restore(centered);
		centeredRuntime.update(1.0f / 60, moved);
		require((tail(centered) - Vector3(0, 2, 0)).length() < 0.00001f, "Center translation or rotation created spurious inertia");
		fixture.write(false, false, false);
		avatar::VrmDocument worldSpace(fixture.path);
		avatar::SpringRuntime worldRuntime(worldSpace);
		worldRuntime.update(1.0f / 60);
		restore(worldSpace);
		worldRuntime.update(1.0f / 60, moved);
		require((tail(worldSpace) - Vector3(0, 2, 0)).length() > 0.01f, "World-space root movement failed to create inertia");
		fixture.write(false, true, true);
		const auto unit = simulate(fixture, 1);
		require((simulate(fixture, 0.25f) - unit).length() < 0.00001f, "Small uniform scale changed local spring motion");
		require((simulate(fixture, 3) - unit).length() < 0.00001f, "Large uniform scale changed local spring motion");
		std::cout << "Spring center, world inertia, uniform scaling, zero time and suspension tests passed\n";
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
