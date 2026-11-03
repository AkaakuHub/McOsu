#ifndef MCOSU_SPRING_RUNTIME_H
#define MCOSU_SPRING_RUNTIME_H

#include "SpringDefinition.h"
#include "SpringMath.h"
#include "Matrices.h"
#include <vector>

namespace avatar
{
class VrmDocument;
class SpringRuntime
{
public:
	explicit SpringRuntime(VrmDocument &document);
	void update(float deltaTime, const Matrix4 &modelToWorld = Matrix4());
	void reset();
	std::size_t chainCount() const { return m_definition.chains.size(); }
	std::size_t jointPairCount() const;

private:
	struct JointState
	{
		Vector3 localTail;
		Vector3 previous;
		Vector3 current;
		bool initialized = false;
	};
	VrmDocument &m_document;
	SpringDefinition m_definition;
	std::vector<std::vector<JointState>> m_states;
	std::vector<bool> m_dynamicColliders;
	std::vector<SpringWorldCollider> m_worldColliders;
};
}

#endif
