#include "VrmPose.h"
#include "VrmDocument.h"
#include "VrmTransforms.h"
#include "PoseMath.h"
#include "cgltf.h"
#include <algorithm>
#include <cmath>

// Elbow pole heuristics adapted from RenIK; see third_party/renik/LICENSE.txt.

namespace avatar
{
namespace
{
float angle(Vector3 left, Vector3 right)
{
	return std::acos(std::max(-1.0f, std::min(1.0f, left.normalize().dot(right.normalize()))));
}

Vector3 elbowDirection(Vector3 target, Vector3 rest, Vector3 handForward, float mirror, float upper, float lower)
{
	const auto direction = target.length() > 0.000001f ? target / target.length() : Vector3(0, -1, 0);
	Matrix4 offset;
	offset.rotateZ(mirror * 60);
	offset.rotateX(15);
	auto axis = rotationBetween(offset * Vector3(0, 1, 0), direction) * (offset * Vector3(1, 0, 0));
	const float positional = Vector3(2, -1.5f * mirror, -mirror).dot(target - rest);
	Matrix4 roll;
	roll.rotate((positional - mirror * 2.094395102f) * 180.0f / 3.141592654f, direction);
	axis = roll * axis;
	const auto base = solveLimb(Vector3(), target, direction.cross(axis), upper, lower);
	const auto forearm = (base.end - base.middle).normalize();
	auto handRejection = handForward - direction * handForward.dot(direction);
	auto lowerRejection = forearm - direction * forearm.dot(direction);
	if (handRejection.length() > 0.0001f && lowerRejection.length() > 0.0001f)
	{
		const float amount = angle(handRejection, lowerRejection) * 0.33f
			* std::abs(handForward.cross(forearm).dot(direction)) * -mirror;
		Matrix4 wristRoll;
		wristRoll.rotate(amount * 180.0f / 3.141592654f, direction);
		axis = wristRoll * axis;
	}
	return direction.cross(axis);
}

Matrix4 twistAround(const Matrix4 &matrix, Vector3 axis, float radians)
{
	Matrix4 twist;
	twist.rotate(radians * 180.0f / 3.141592654f, axis);
	auto result = twist * matrix;
	result[12] = matrix[12];
	result[13] = matrix[13];
	result[14] = matrix[14];
	return result;
}
}

void VrmPose::moveArm(const std::string &side, const Matrix4 &target)
{
	const auto rest = [this](const std::string &name) { return m_restWorld[m_document.humanBones().at(name)]; };
	auto &upper = bone(side + "UpperArm");
	auto &lower = bone(side + "LowerArm");
	auto &hand = bone(side + "Hand");
	auto &shoulder = bone(side + "Shoulder");
	const auto restHand = rest(side + "Hand");
	const auto fingerForward = position(rest(side + "MiddleProximal")) - position(restHand);
	auto palmUp = (position(rest(side + "IndexProximal")) - position(restHand)).cross(
		position(rest(side + "LittleProximal")) - position(restHand));
	if (palmUp.y < 0) palmUp = -palmUp;
	const auto offset = handFrame(fingerForward, palmUp).invert() * orientation(restHand);
	Matrix4 turn;
	turn.rotateY(180);
	const auto targetFrame = orientation(target) * turn;
	const auto desiredRotation = targetFrame * offset;
	const auto targetPosition = position(target);
	const auto upperLength = (position(world(lower)) - position(world(upper))).length();
	const auto lowerLength = (position(world(hand)) - position(world(lower))).length();
	const auto shoulderPosition = position(world(shoulder));
	const auto shoulderDirection = position(world(upper)) - shoulderPosition;
	const auto reaching = targetPosition - shoulderPosition;
	const float extension = std::max(0.0f, std::min(1.0f,
		((targetPosition - position(world(upper))).length() / (upperLength + lowerLength) - 0.75f) / 0.25f));
	if (extension > 0)
	{
		const float shoulderAngle = std::min(30.0f, angle(shoulderDirection, reaching) * 180.0f / 3.141592654f) * extension;
		auto axis = shoulderDirection.cross(reaching);
		if (axis.length() > 0.000001f)
		{
			const auto original = world(shoulder);
			setWorld(shoulder, twistAround(original, axis.normalize(), shoulderAngle * 3.141592654f / 180.0f));
		}
	}
	const auto start = position(world(upper));
	const auto restVector = position(restHand) - position(rest(side + "UpperArm"));
	const auto bend = elbowDirection(targetPosition - start, restVector,
		targetFrame * Vector3(0, 0, 1), side == "left" ? 1.0f : -1.0f, upperLength, lowerLength);
	moveLimb(side, true, targetPosition, bend);
	const auto lowerWorld = world(lower);
	const auto handWorld = world(hand);
	const auto forearmAxis = (position(handWorld) - position(lowerWorld)).normalize();
	const auto bindUp = orientation(restHand).invert() * palmUp.normalize();
	auto currentUp = orientation(handWorld) * bindUp;
	auto desiredUp = desiredRotation * bindUp;
	currentUp -= forearmAxis * currentUp.dot(forearmAxis);
	desiredUp -= forearmAxis * desiredUp.dot(forearmAxis);
	if (currentUp.length() > 0.0001f && desiredUp.length() > 0.0001f)
	{
		currentUp.normalize();
		desiredUp.normalize();
		const float twist = std::atan2(forearmAxis.dot(currentUp.cross(desiredUp)), currentUp.dot(desiredUp));
		const auto upperWorld = world(upper);
		const auto upperAxis = (position(lowerWorld) - position(upperWorld)).normalize();
		setWorld(upper, twistAround(upperWorld, upperAxis, twist / 3));
		setWorld(lower, twistAround(lowerWorld, forearmAxis, twist * 2 / 3));
	}
	auto handRotation = desiredRotation;
	const auto handPosition = position(world(hand));
	handRotation[12] = handPosition.x;
	handRotation[13] = handPosition.y;
	handRotation[14] = handPosition.z;
	setWorld(hand, handRotation);
}
}
