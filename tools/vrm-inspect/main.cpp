#include "VrmDocument.h"
#include "cgltf.h"
#include <exception>
#include <iostream>

int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	try
	{
		avatar::VrmDocument model(argv[1]);
		const auto &scene = model.scene();
		std::cout << "nodes=" << scene.nodes_count << " meshes=" << scene.meshes_count
			<< " skins=" << scene.skins_count << " humanoidBones=" << model.humanBones().size() << '\n';
	}
	catch (const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
