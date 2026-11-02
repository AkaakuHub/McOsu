#include "FootLocomotion.h"
#include "LocomotionMath.h"
#include <cmath>

namespace avatar
{
using namespace locomotion;

void FootLocomotion::configure(const Config &config) { m_config = config; reset(); }
void FootLocomotion::reset()
{
	m_initialized = false;
	m_history.clear();
	m_motionWeight = 0;
	m_time = 0;
	m_floorY = 0;
	m_lastStep = 1;
	m_recovering.fill(false);
	for (auto &foot : m_feet) foot = Foot();
}

void FootLocomotion::rebase(const Matrix4 &delta)
{
	const float deltaYaw = yaw(delta);
	for (int i = 0; i < 2; ++i)
	{
		m_feet[i].pose = delta * m_feet[i].pose;
		m_steps[i].start = delta * m_steps[i].start;
		m_steps[i].end = delta * m_steps[i].end;
	}
	for (auto &sample : m_history)
	{
		sample.position = delta * sample.position + position(delta);
		sample.velocity = delta * sample.velocity;
		sample.yaw = angle(sample.yaw + deltaYaw);
	}
	m_floorY = (delta * Vector3(0, m_floorY, 0) + position(delta)).y;
}

const std::array<FootLocomotion::Foot, 2> &FootLocomotion::update(const Input &input)
{
	if (input.hasTrackingSpaceDelta && m_initialized) rebase(input.trackingSpaceDelta);
	const float dt = std::isfinite(input.dt) ? clamp(input.dt, 0, .1f) : 0;
	const float scale = Vector3(input.modelToTracking[0], input.modelToTracking[1], input.modelToTracking[2]).length();
	const float leg = std::max(.01f, std::min(m_config.legLength[0], m_config.legLength[1]) * scale);
	Matrix4 body = rigid(input.modelToTracking);
	Vector3 center = input.hipsWorld;
	center.y = m_floorY;
	const float heading = yaw(body);
	Vector3 velocity;
	float angularVelocity = 0;
	if (!m_history.empty() && dt > 0)
	{
		velocity = (center - m_history.back().position) / dt;
		angularVelocity = angle(heading - m_history.back().yaw) / dt;
	}
	m_time += dt;
	if (dt > 0 || m_history.empty())
	{
		Sample sample; sample.time=m_time; sample.position=center; sample.yaw=heading;
		sample.velocity=velocity; sample.angularVelocity=angularVelocity; sample.duration=dt;
		m_history.push_back(sample);
	}
	while (m_history.size() > 1 && m_time - m_history.front().time > 1) m_history.pop_front();
	Vector3 mean, meanVelocity;
	float sine=0, cosine=0, meanAngular=0, total=0;
	for (const auto &sample : m_history)
	{
		const float weight = std::max(.000001f, sample.duration);
		mean += sample.position * weight; meanVelocity += sample.velocity * weight;
		sine += std::sin(sample.yaw)*weight; cosine += std::cos(sample.yaw)*weight;
		meanAngular += sample.angularVelocity*weight; total += weight;
	}
	mean /= total; meanVelocity /= total; meanAngular /= total;
	m_motionWeight = clamp(std::max(std::max(meanVelocity.length()/.5f, std::abs(meanAngular)/.34906585f), m_motionWeight-dt), 0, 1);
	center = mean*(1-m_motionWeight)+center*m_motionWeight;
	float filteredYaw = std::atan2(sine,cosine);
	filteredYaw += angle(heading-filteredYaw)*m_motionWeight;
	Matrix4 targetBody = rotation(filteredYaw);
	position(targetBody, center - targetBody * (planar(m_config.restPelvis)*scale));
	std::array<Matrix4,2> desired;
	for (int i=0; i<2; ++i)
	{
		Matrix4 local = rigid(m_config.restFeet[i]);
		position(local, position(local)*scale);
		desired[i] = targetBody * local;
		Vector3 p=position(desired[i]); p.y=m_floorY+position(m_config.restFeet[i]).y*scale; position(desired[i],p);
		if (!m_initialized) m_feet[i].pose=desired[i];
		if (m_feet[i].tracked && !input.trackedFeet[i].valid) m_recovering[i]=true;
		m_feet[i].tracked=input.trackedFeet[i].valid;
		if (m_feet[i].tracked)
		{
			m_feet[i].pose=input.trackedFeet[i].pose;
			m_feet[i].stepping=false;
			m_recovering[i]=false;
		}
	}
	m_initialized=true;
	const bool moving=m_feet[0].stepping || m_feet[1].stepping;
	const bool fullyTracked=input.pelvisTracked && m_feet[0].tracked && m_feet[1].tracked;
	if (!moving && !fullyTracked && dt > 0)
	{
		const Vector3 left=planar(position(m_feet[0].pose));
		const Vector3 across=planar(position(m_feet[1].pose))-left;
		const Vector3 balance=planar(input.balancePoint);
		const float square=across.dot(across);
		const float fraction=square>0 ? clamp((balance-left).dot(across)/square,0,1) : 0;
		const float imbalance=(balance-left-across*fraction).length();
		float errors[2]={0,0};
		for (int i=0; i<2; ++i)
		{
			if (m_feet[i].tracked) continue;
			const float displacement=planar(position(desired[i])-position(m_feet[i].pose)).length();
			const float turn=std::abs(angle(yaw(desired[i])-yaw(m_feet[i].pose)));
			const float reach=(input.hipsWorld-position(m_feet[i].pose)).length()/(m_config.legLength[i]*scale);
			const float threshold=leg*(.16f+.08f*m_motionWeight);
			errors[i]=std::max(displacement/threshold, turn/.61f);
			if (m_recovering[i]) errors[i]=std::max(errors[i],1.01f);
			if (imbalance > leg*.35f) errors[i]=std::max(errors[i],displacement/(leg*.12f));
			if (reach > 1.08f && displacement > leg*.03f) errors[i]=std::max(errors[i], reach/1.08f);
		}
		const int next=errors[0]>errors[1] ? 0 : errors[1]>errors[0] ? 1 : 1-m_lastStep;
		if (errors[next] > 1)
		{
			Step &step=m_steps[next]; step.start=m_feet[next].pose; step.end=desired[next]; step.elapsed=0;
			Vector3 prediction=planar(meanVelocity)*.08f;
			if (prediction.length()>leg*.2f) prediction*=leg*.2f/prediction.length();
			Vector3 p=position(step.end)+prediction;
			position(step.end,p);
			const float distance=(position(step.end)-position(step.start)).length();
			step.height=std::min(leg*.08f,distance*.25f);
			step.duration=clamp(.18f+distance*.15f,.18f,.3f);
			m_feet[next].stepping=true; m_recovering[next]=false; m_lastStep=next;
		}
	}
	for (int i=0; i<2; ++i)
	{
		if (!m_feet[i].stepping || m_feet[i].tracked) continue;
		Step &step=m_steps[i]; step.elapsed=std::min(step.duration,step.elapsed+dt);
		float t=step.elapsed/step.duration;
		m_feet[i].pose=interpolate(step.start,step.end,t*t*(3-2*t));
		m_feet[i].pose[13]+=std::sin(t*3.141592654f)*step.height;
		if (t>=1) { m_feet[i].pose=step.end; m_feet[i].stepping=false; }
	}
	return m_feet;
}
}
