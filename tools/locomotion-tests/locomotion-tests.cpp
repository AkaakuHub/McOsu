#include "Locomotion/FootLocomotion.h"
#include "Locomotion/LocomotionMath.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using avatar::FootLocomotion;
using namespace avatar::locomotion;
namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
bool near(Vector3 a,Vector3 b,float epsilon=.0001f) { return (a-b).length()<epsilon; }
struct Scene
{
	FootLocomotion solver;
	FootLocomotion::Input input;
	Scene(float hz=90, bool reversed=false)
	{
		FootLocomotion::Config config;
		config.restFeet[0].translate(-.12f,.06f,0); config.restFeet[1].translate(.12f,.06f,0);
		if(reversed) std::swap(config.restFeet[0],config.restFeet[1]);
		config.restPelvis=Vector3(0,.95f,0); config.legLength={{.95f,.95f}}; solver.configure(config);
		input.dt=1/hz; input.hipsWorld=Vector3(0,.95f,0); input.balancePoint=Vector3(0,0,0);
		input.head.translate(0,1.65f,0); input.hands={{Vector3(-.4f,1,0),Vector3(.4f,1,0)}};
		solver.update(input);
	}
	const std::array<FootLocomotion::Foot,2>& update()
	{
		const auto &feet=solver.update(input);
		require(!(feet[0].stepping&&feet[1].stepping),"both estimated feet stepped simultaneously");
		for(const auto &foot:feet) for(int n=0;n<16;++n) require(std::isfinite(foot.pose[n]),"nonfinite pose");
		return feet;
	}
};
void stationary()
{
	Scene s; auto initial=s.update();
	for(int i=0;i<900;++i)
	{
		s.input.hipsWorld.x=std::sin(i*.31f)*.002f;
		s.input.balancePoint.x=s.input.hipsWorld.x;
		s.input.head[12]=std::sin(i*.17f)*.15f;
		const auto &feet=s.update();
		for(int j=0;j<2;++j) require(near(position(feet[j].pose),position(initial[j].pose)),"jitter or head lean moved planted foot");
	}
	for(int i=0;i<180;++i) { s.input.hipsWorld.y=.5f; const auto &feet=s.update(); require(!feet[0].stepping&&!feet[1].stepping,"crouch caused step"); }
	for(int i=0;i<180;++i) { s.input.hipsWorld.x=.1f; s.input.balancePoint.x=.1f; const auto &feet=s.update(); require(!feet[0].stepping&&!feet[1].stepping,"small weight shift caused step"); }
}
Vector3 walking(float hz)
{
	Scene s(hz); int steps=0; bool wasMoving=false;
	for(int i=0;i<int(hz*4);++i)
	{
		float x=std::min(.9f,i/hz*.3f); s.input.hipsWorld.x=x; s.input.balancePoint.x=x; s.input.modelToTracking[12]=x;
		const auto &feet=s.update(); bool moving=feet[0].stepping||feet[1].stepping;
		if(moving&&!wasMoving) ++steps; wasMoving=moving;
	}
	const auto &feet=s.update(); require(steps>=2,"translation did not produce alternating steps");
	return (position(feet[0].pose)+position(feet[1].pose))*.5f;
}
void turning()
{
	Scene s; int stepped=0;
	for(int i=0;i<360;++i)
	{
		s.input.modelToTracking=rotation(std::min(1.57f,i/90.f));
		const auto &feet=s.update(); if(feet[0].stepping||feet[1].stepping) ++stepped;
	}
	require(stepped>0,"turn did not cause a step");
	const auto &feet=s.update(); require(std::abs(angle(yaw(feet[0].pose)-1.57f))<.62f,"turn did not align foot");
}
void trackers()
{
	Scene s;
	s.input.trackedFeet[0].valid=true; s.input.trackedFeet[0].pose.rotateX(35).translate(-.2f,.4f,.1f);
	for(int i=0;i<180;++i)
	{
		s.input.hipsWorld.x=.7f; s.input.balancePoint.x=.7f;
		const auto &feet=s.update(); require(feet[0].pose==s.input.trackedFeet[0].pose,"tracked foot was modified");
	}
	s.input.trackedFeet[1].valid=true; s.input.trackedFeet[1].pose.translate(.3f,.2f,-.1f); s.input.pelvisTracked=true;
	const auto both=s.update(); require(both[0].tracked&&both[1].tracked&&!both[0].stepping&&!both[1].stepping,"full tracking did not disable estimation");
	s.input.trackedFeet[0].valid=false;
	const auto &loss=s.update(); require((position(loss[0].pose)-position(both[0].pose)).length()<.03f,"tracking loss snapped foot");
	for(int i=0;i<120;++i) s.update();
	require(std::abs(s.update()[0].pose[13]-.06f)<.0001f,"lost tracker did not return foot to floor");
}
void movingPlatform()
{
	Scene s; s.input.hipsWorld.x=.6f; s.input.balancePoint.x=.6f;
	auto before=s.update(); require(before[0].stepping||before[1].stepping,"platform test needs active step");
	Matrix4 delta; delta.rotateY(35); delta.translate(1,.3f,2);
	s.input.dt=0; s.input.hasTrackingSpaceDelta=true; s.input.trackingSpaceDelta=delta;
	s.input.modelToTracking=delta; s.input.hipsWorld=delta*s.input.hipsWorld+position(delta); s.input.balancePoint=delta*s.input.balancePoint+position(delta);
	const auto after=s.update();
	for(int i=0;i<2;++i) require(near(position(after[i].pose),delta*position(before[i].pose)+position(delta)),"platform delta did not transform active step");
	s.input.hasTrackingSpaceDelta=false; s.input.dt=1/90.f;
	for(int i=0;i<450;++i) s.update();
	const auto &feet=s.update(); for(const auto &foot:feet) require(std::abs(foot.pose[13]-.36f)<.001f,"platform floor height not preserved");
}
void landingConstraints(bool reversed, bool turn)
{
	Scene s(90,reversed); auto previous=s.update(); int landings=0;
	std::array<Vector3,2> starts={{position(previous[0].pose),position(previous[1].pose)}};
	for(int frame=0;frame<900;++frame)
	{
		float x=turn ? 0 : std::min(.8f,frame/90.f*.25f);
		s.input.hipsWorld.x=x; s.input.balancePoint.x=x;
		s.input.modelToTracking=rotation(turn ? std::min(3.14f,frame/90.f*.7f) : 0);
		const auto current=s.update();
		for(int foot=0;foot<2;++foot)
		{
			if(current[foot].stepping&&!previous[foot].stepping) starts[foot]=position(previous[foot].pose);
			if(!current[foot].stepping&&previous[foot].stepping)
			{
				++landings;
				Vector3 across=rotation(yaw(current[foot].pose))*Vector3(reversed ? -1.f : 1.f,0,0);
				Vector3 outward=across*(foot==0 ? -1.f : 1.f);
				float spacing=planar(position(current[foot].pose)-position(current[1-foot].pose)).dot(outward);
				require(spacing>=.119f,"landing crossed or crowded support foot");
				require(planar(position(current[foot].pose)-starts[foot]).length()<=.761f,"landing exceeded maximum step reach");
			}
		}
		previous=current;
	}
	require(landings>=2,"landing constraint scenario did not step");
}
void rebase()
{
	Scene s; auto before=s.update(); Matrix4 delta; delta.rotateY(80); delta.translate(2,0,3);
	s.input.hasTrackingSpaceDelta=true; s.input.trackingSpaceDelta=delta;
	s.input.modelToTracking=delta; s.input.hipsWorld=delta*s.input.hipsWorld+position(delta); s.input.balancePoint=delta*s.input.balancePoint+position(delta);
	const auto after=s.update();
	for(int i=0;i<2;++i) require(near(position(after[i].pose),delta*position(before[i].pose)+position(delta)),"recenter did not transform planted foot");
	s.input.hasTrackingSpaceDelta=false;
	for(int i=0;i<90;++i) { const auto &feet=s.update(); require(!feet[0].stepping&&!feet[1].stepping,"recenter history caused spurious step"); }
}
}
int main()
{
	try
	{
		stationary(); turning(); trackers(); rebase(); movingPlatform();
		landingConstraints(false,false); landingConstraints(true,false);
		landingConstraints(false,true); landingConstraints(true,true);
		Vector3 p30=walking(30),p60=walking(60),p90=walking(90);
		require(near(p30,p60,.08f)&&near(p60,p90,.08f),"frame rate changed walking result");
		std::cout<<"locomotion tests passed\n"; return EXIT_SUCCESS;
	}
	catch(const std::exception &error) { std::cerr<<error.what()<<"\n"; return EXIT_FAILURE; }
}
