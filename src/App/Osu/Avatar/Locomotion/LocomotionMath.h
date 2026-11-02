#ifndef MCOSU_LOCOMOTION_MATH_H
#define MCOSU_LOCOMOTION_MATH_H
#include "Matrices.h"
#include <algorithm>
#include <cmath>
namespace avatar { namespace locomotion
{
inline float clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
inline Vector3 position(const Matrix4 &m) { return Vector3(m[12], m[13], m[14]); }
inline void position(Matrix4 &m, Vector3 p) { m[12] = p.x; m[13] = p.y; m[14] = p.z; }
inline Vector3 planar(Vector3 p) { p.y = 0; return p; }
inline float yaw(const Matrix4 &m) { return std::atan2(m[8], m[10]); }
inline float angle(float a) { return std::atan2(std::sin(a), std::cos(a)); }
inline Matrix4 rotation(float yaw) { Matrix4 m; m.rotateY(yaw * 57.295779513f); return m; }
inline Matrix4 rigid(Matrix4 m)
{
	for (int c = 0; c < 3; ++c)
	{
		Vector3 v(m[c * 4], m[c * 4 + 1], m[c * 4 + 2]);
		const float length = v.length();
		if (length > 0) v /= length;
		m[c * 4] = v.x; m[c * 4 + 1] = v.y; m[c * 4 + 2] = v.z;
	}
	return m;
}
struct Quaternion
{
	float x, y, z, w;
	explicit Quaternion(const Matrix4 &m)
	{
		const float trace = m[0] + m[5] + m[10];
		if (trace > 0) { float s = std::sqrt(trace + 1) * 2; w = .25f*s; x=(m[6]-m[9])/s; y=(m[8]-m[2])/s; z=(m[1]-m[4])/s; }
		else if (m[0] > m[5] && m[0] > m[10]) { float s=std::sqrt(1+m[0]-m[5]-m[10])*2; w=(m[6]-m[9])/s; x=.25f*s; y=(m[4]+m[1])/s; z=(m[8]+m[2])/s; }
		else if (m[5] > m[10]) { float s=std::sqrt(1+m[5]-m[0]-m[10])*2; w=(m[8]-m[2])/s; x=(m[4]+m[1])/s; y=.25f*s; z=(m[9]+m[6])/s; }
		else { float s=std::sqrt(1+m[10]-m[0]-m[5])*2; w=(m[1]-m[4])/s; x=(m[8]+m[2])/s; y=(m[9]+m[6])/s; z=.25f*s; }
	}
};
inline Matrix4 interpolate(const Matrix4 &a, const Matrix4 &b, float t)
{
	Quaternion p(a), q(b);
	float dot = p.x*q.x+p.y*q.y+p.z*q.z+p.w*q.w;
	if (dot < 0) { q.x=-q.x; q.y=-q.y; q.z=-q.z; q.w=-q.w; dot=-dot; }
	float u=1-t, v=t;
	if (dot < .9995f) { float theta=std::acos(clamp(dot,-1,1)); u=std::sin((1-t)*theta)/std::sin(theta); v=std::sin(t*theta)/std::sin(theta); }
	float x=p.x*u+q.x*v, y=p.y*u+q.y*v, z=p.z*u+q.z*v, w=p.w*u+q.w*v;
	float length=std::sqrt(x*x+y*y+z*z+w*w); x/=length; y/=length; z/=length; w/=length;
	Matrix4 m;
	m[0]=1-2*(y*y+z*z); m[1]=2*(x*y+z*w); m[2]=2*(x*z-y*w);
	m[4]=2*(x*y-z*w); m[5]=1-2*(x*x+z*z); m[6]=2*(y*z+x*w);
	m[8]=2*(x*z+y*w); m[9]=2*(y*z-x*w); m[10]=1-2*(x*x+y*y);
	position(m, position(a)*(1-t)+position(b)*t);
	return m;
}
}}
#endif
