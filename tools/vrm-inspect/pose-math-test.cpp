#include "PoseMath.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}

int main()
{
	try
	{
		for (const auto target : {Vector3(0.3f, 0.2f, 0.1f), Vector3(3, 0, 0), Vector3(0, 0, 0), Vector3(0, -0.3f, 0)})
		{
			const auto limb = avatar::solveLimb(Vector3(), target, Vector3(0, -1, 0), 0.3f, 0.25f);
			require(std::abs(limb.middle.length() - 0.3f) < 0.00001f, "Upper bone length changed");
			require(std::abs((limb.end - limb.middle).length() - 0.25f) < 0.00001f, "Lower bone length changed");
			if (target.length() > 0.05f && target.length() < 0.55f)
				require((limb.end - target).length() < 0.00001f, "Reachable hand target was missed");
		}
		for (const auto target : {Vector3(0, 1, 0), Vector3(-1, 0, 0), Vector3(1, 0, 0)})
		{
			const auto rotated = avatar::rotationBetween(Vector3(1, 0, 0), target) * Vector3(1, 0, 0);
			require((rotated - target).length() < 0.00001f, "Bone rotation missed target direction");
		}
		std::cout << "Limb reach, bone lengths, singular targets and rotations passed\n";
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
