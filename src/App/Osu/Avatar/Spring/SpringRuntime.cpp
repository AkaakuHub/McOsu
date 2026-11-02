#include "SpringRuntime.h"
#include "SpringMath.h"
#include "VrmDocument.h"
#include "VrmTransforms.h"
#include "PoseMath.h"
#include "cgltf.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace avatar
{
namespace
{
float maxScale(const Matrix4 &matrix)
{
	return std::max(Vector3(matrix[0], matrix[1], matrix[2]).length(),
		std::max(Vector3(matrix[4], matrix[5], matrix[6]).length(), Vector3(matrix[8], matrix[9], matrix[10]).length()));
}
}

SpringRuntime::SpringRuntime(VrmDocument &document) : m_document(document), m_definition(readSpringDefinition(document.scene()))
{
	const auto &scene = document.scene();
	for (const auto &chain : m_definition.chains)
	{
		std::vector<JointState> states;
		for (std::size_t index = 0; index + 1 < chain.joints.size(); ++index)
		{
			const auto head = world(scene.nodes[chain.joints[index].node]);
			const auto tail = position(world(scene.nodes[chain.joints[index + 1].node]));
			JointState state;
			state.localTail = point(Matrix4(head).invert(), tail);
			if (state.localTail.length() < 0.0000001f) throw std::runtime_error("SpringBone joint pair has zero length");
			states.push_back(state);
		}
		m_states.push_back(states);
	}
}

std::size_t SpringRuntime::jointPairCount() const
{
	std::size_t count = 0;
	for (const auto &states : m_states) count += states.size();
	return count;
}

void SpringRuntime::reset()
{
	for (auto &states : m_states) for (auto &state : states) state.initialized = false;
}

void SpringRuntime::update(float deltaTime, const Matrix4 &modelToWorld)
{
	if (!std::isfinite(deltaTime) || deltaTime < 0) throw std::runtime_error("SpringBone delta time must be finite and nonnegative");
	if (deltaTime > 0.1f) { reset(); deltaTime = 0; }
	auto &scene = m_document.scene();
	const auto worldToModel = Matrix4(modelToWorld).invert();
	for (std::size_t chainIndex = 0; chainIndex < m_definition.chains.size(); ++chainIndex)
	{
		const auto &chain = m_definition.chains[chainIndex];
		const auto center = chain.center >= 0 ? modelToWorld * world(scene.nodes[chain.center]) : Matrix4();
		const auto inverseCenter = Matrix4(center).invert();
		for (std::size_t index = 0; index < m_states[chainIndex].size(); ++index)
		{
			auto &state = m_states[chainIndex][index];
			const auto &settings = chain.joints[index];
			auto &node = scene.nodes[settings.node];
			const auto headMatrix = modelToWorld * world(node);
			const auto head = position(headMatrix);
			const auto restTail = point(headMatrix, state.localTail);
			const float length = (restTail - head).length();
			const auto direction = (restTail - head).normalize();
			if (!state.initialized)
			{
				state.previous = state.current = point(inverseCenter, restTail);
				state.initialized = true;
			}
			const auto current = point(center, state.current);
			const auto previous = point(center, state.previous);
			const float scale = maxScale(headMatrix);
			auto tail = integrateSpringTail(head, current, previous, direction, length, settings, deltaTime, scale);
			if (deltaTime > 0) for (const auto colliderIndex : chain.colliders)
			{
				const auto &collider = m_definition.colliders[colliderIndex];
				const auto matrix = modelToWorld * world(scene.nodes[collider.node]);
				const SpringWorldCollider shape = {point(matrix, collider.offset), point(matrix, collider.tail), collider.radius * maxScale(matrix)};
				tail = collideSpringTail(head, tail, length, settings.radius * scale, shape, direction);
			}
			if (!std::isfinite(tail.x) || !std::isfinite(tail.y) || !std::isfinite(tail.z))
				throw std::runtime_error("SpringBone produced a non-finite tail");
			if (deltaTime > 0)
			{
				state.previous = state.current;
				state.current = point(inverseCenter, tail);
			}
			auto rotated = rotationBetween(direction, tail - head) * headMatrix;
			rotated[12] = head.x;
			rotated[13] = head.y;
			rotated[14] = head.z;
			setWorld(node, worldToModel * rotated);
		}
	}
}
}
