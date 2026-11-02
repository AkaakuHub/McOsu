#include "Spring/SpringRuntime.h"
#include "Spring/SpringDefinition.h"
#include "VrmDocument.h"
#include "VrmTransforms.h"
#include "cgltf.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	try
	{
		avatar::VrmDocument model(argv[1]);
		avatar::SpringRuntime runtime(model);
		const auto definition = avatar::readSpringDefinition(model.scene());
		std::vector<Matrix4> local, restWorld;
		for (std::size_t index = 0; index < model.scene().nodes_count; ++index)
		{
			float matrix[16];
			cgltf_node_transform_local(&model.scene().nodes[index], matrix);
			local.emplace_back(matrix);
			restWorld.push_back(avatar::world(model.scene().nodes[index]));
		}
		const auto restore = [&]() {
			for (std::size_t index = 0; index < local.size(); ++index)
			{
				std::copy(local[index].get(), local[index].get() + 16, model.scene().nodes[index].matrix);
				model.scene().nodes[index].has_matrix = true;
			}
		};
		float maxMotion = 0, maxLengthError = 0;
		for (int step = 0; step < 180; ++step)
		{
			restore();
			Matrix4 root;
			root.translate(std::sin(step * 0.1f) * 0.03f, 0, 0);
			runtime.update(1.0f / 60, root);
			for (const auto &chain : definition.chains)
				for (std::size_t index = 0; index + 1 < chain.joints.size(); ++index)
				{
					const auto headIndex = chain.joints[index].node, tailIndex = chain.joints[index + 1].node;
					const auto head = avatar::position(avatar::world(model.scene().nodes[headIndex]));
					const auto tail = avatar::position(avatar::world(model.scene().nodes[tailIndex]));
					const auto restHead = avatar::position(restWorld[headIndex]), restTail = avatar::position(restWorld[tailIndex]);
					maxLengthError = std::max(maxLengthError, std::abs((tail - head).length() - (restTail - restHead).length()));
					maxMotion = std::max(maxMotion, (tail - restTail).length());
					if (!std::isfinite(tail.x) || !std::isfinite(tail.y) || !std::isfinite(tail.z))
						throw std::runtime_error("Non-finite spring pose");
				}
		}
		restore();
		runtime.update(1.0f);
		float resetError = 0;
		for (std::size_t index = 0; index < restWorld.size(); ++index)
			resetError = std::max(resetError,
				(avatar::position(avatar::world(model.scene().nodes[index])) - avatar::position(restWorld[index])).length());
		std::cout << "chains=" << runtime.chainCount() << " jointPairs=" << runtime.jointPairCount()
			<< " frames=180 maxMotion=" << maxMotion << " maxLengthError=" << maxLengthError << " resetError=" << resetError << '\n';
		if (runtime.jointPairCount() == 0 || maxMotion < 0.00001f || maxLengthError > 0.0001f || resetError > 0.0001f)
			throw std::runtime_error("Spring runtime motion, bone length or pause reset audit failed");
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
