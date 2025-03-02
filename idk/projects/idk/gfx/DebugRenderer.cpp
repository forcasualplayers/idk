#include "stdafx.h"
#include "DebugRenderer.h"

#include <gfx/MeshFactory.h>
#include <math/matrix_transforms.inl>
#include <ds/span.inl>
#include <gfx/Camera.h>


namespace idk
{
	void DebugRenderer::Draw(RscHandle<Mesh> mesh, const mat4& transform, const color& c, seconds duration, bool depth_test)
	{
		debug_info.emplace_back(DebugInfo{ mesh, transform, c, duration, depth_test });
	}
	void DebugRenderer::Draw(const vec3& point, const color& c, seconds duration, bool depth_test)
	{
		const mat4 tfm = translate(point) * mat4 { scale(vec3{ 0.1f }) };
		Draw(Mesh::defaults[MeshType::Sphere], tfm, c, duration, depth_test);
	}
	void DebugRenderer::Draw(const aabb& bounding_box, const color& c, seconds duration, bool depth_test)
	{
		Draw(box{ bounding_box.center(), bounding_box.extents() }, c, duration, depth_test);
	}
	void DebugRenderer::Draw(const box& oriented_box, const color& c, seconds duration, bool depth_test)
	{
		const mat4 tfm = translate(oriented_box.center) * mat4 { oriented_box.axes() } * scale(oriented_box.extents);
		Draw(Mesh::defaults[MeshType::DbgBox], tfm, c, duration, depth_test);
		// Draw(ray{ vec3{0, 0,0},  oriented_box.axes() * vec3{1,0,0} });
	}

	void DebugRenderer::Draw(const capsule& capsule, const color& c, seconds duration, bool depth_test)
	{
		//T * R * S
		vec3 rightVec = vec3{ 0,1,0 }.cross(capsule.dir); //Using up as a reference

		if (rightVec.length_sq() == 0 ||  abs(rightVec.length_sq()) <= epsilon)
			rightVec = vec3{ 1,0,0 }.cross(capsule.dir); //Use front as a reference instead if cannot build cross product with up
		rightVec.normalize();
		const vec3 frontVec = capsule.dir.cross(rightVec).get_normalized();

		float minSpherePoint = ((capsule.height * 0.5f) - capsule.radius);
		minSpherePoint = minSpherePoint < 0 ? 0 : minSpherePoint;
		const auto capsule_sphere_top_tfm  = translate(capsule.center+ (capsule.dir.get_normalized()* minSpherePoint)) * mat4 { mat3{capsule.dir,rightVec,frontVec  }} * scale(vec3(capsule.radius*2));
		const auto capsule_sphere_topB_tfm = translate(capsule.center+ (capsule.dir.get_normalized()* minSpherePoint)) * mat4 { mat3{ frontVec,capsule.dir,rightVec }} * scale(vec3(capsule.radius*2));
		const auto capsule_sphere_topC_tfm = translate(capsule.center+ (capsule.dir.get_normalized()* minSpherePoint)) * mat4 { mat3{ rightVec,frontVec,capsule.dir }} * scale(vec3(capsule.radius*2));

		const auto capsule_sphere_bot_tfm  = translate(capsule.center- (capsule.dir.get_normalized()* minSpherePoint)) * mat4 { mat3{ capsule.dir,rightVec,frontVec }} * scale(vec3(capsule.radius*2));
		const auto capsule_sphere_botB_tfm = translate(capsule.center- (capsule.dir.get_normalized()* minSpherePoint)) * mat4 { mat3{ frontVec,capsule.dir,rightVec }} * scale(vec3(capsule.radius*2));
		const auto capsule_sphere_botC_tfm = translate(capsule.center- (capsule.dir.get_normalized()* minSpherePoint)) * mat4 { mat3{ rightVec,frontVec,capsule.dir }} * scale(vec3(capsule.radius*2));

		const vec3 ptOrigin			= capsule.center - (capsule.dir.get_normalized()* capsule.height*0.5f); //Bottom of sphere
		const vec3 ptVelocity		= capsule.dir.get_normalized() * capsule.height;
		const float sideVelocity	= capsule.height * 0.5f - capsule.radius  < 0 ? 0 : capsule.height * 0.5f - capsule.radius ; //For the sides
		const vec3 sidePt			= capsule.center - capsule.dir.get_normalized() * sideVelocity;
		const vec3 ptRightOrigin	= sidePt + rightVec * capsule.radius;
		const vec3 ptLeftOrigin		= sidePt - rightVec * capsule.radius;
		const vec3 ptFrontOrigin	= sidePt + frontVec * capsule.radius;
		const vec3 ptBackOrigin		= sidePt - frontVec * capsule.radius;

		Draw(Mesh::defaults[MeshType::Circle], capsule_sphere_top_tfm , c, duration, depth_test);
		Draw(Mesh::defaults[MeshType::Circle], capsule_sphere_topB_tfm, c, duration, depth_test);
		Draw(Mesh::defaults[MeshType::Circle], capsule_sphere_topC_tfm, c, duration, depth_test);  
		Draw(Mesh::defaults[MeshType::Circle], capsule_sphere_bot_tfm , c, duration, depth_test);  
		Draw(Mesh::defaults[MeshType::Circle], capsule_sphere_botB_tfm, c, duration, depth_test);  
		Draw(Mesh::defaults[MeshType::Circle], capsule_sphere_botC_tfm, c, duration, depth_test);  
		Draw(ptOrigin		, ptOrigin		+  ptVelocity									 , c, duration, depth_test);	//Mid
		Draw(ptRightOrigin	, ptRightOrigin	+ capsule.dir.get_normalized() * sideVelocity * 2, c, duration, depth_test);	//Sides
		Draw(ptLeftOrigin	, ptLeftOrigin	+ capsule.dir.get_normalized() * sideVelocity * 2, c, duration, depth_test);	//Sides
		Draw(ptFrontOrigin	, ptFrontOrigin	+ capsule.dir.get_normalized() * sideVelocity * 2, c, duration, depth_test);	//Sides
		Draw(ptBackOrigin	, ptBackOrigin	+ capsule.dir.get_normalized() * sideVelocity * 2, c, duration, depth_test);	//Sides

	}

	void DebugRenderer::Draw(const ray& ray, const color& c, seconds duration, bool depth_test)
	{
		//mat4 tfm = translate(ray.origin) * mat4(rotate(ray.direction))
		const auto line_tfm = look_at(ray.origin + ray.velocity / 2, ray.origin + ray.velocity, vec3{ 0,1,0 }) * mat4 { scale(vec3{ ray.velocity.length() / 2 }) };
		Draw(Mesh::defaults[MeshType::Line], line_tfm, c, duration, depth_test);

		const auto len = ray.velocity.length();
		const auto dir = ray.velocity / len;
		
		// const float offset = min(len * 0.2f, 0.2f);
		const auto orient_fk = [](auto z_prime)
		{
			const auto axis = vec3{ 0, 0, 1 }.cross(z_prime);
			const auto angle = acos(z_prime.z);
			return rotate(axis, angle);
		};
		
		const auto orient_tfm = mat4{ mat3{ orient(dir) } };
		const auto arrow_tfm = translate(ray.origin + ray.velocity) * orient_tfm * scale(vec3{ 1, 1, 1 });
		Draw(Mesh::defaults[MeshType::DbgArrow], arrow_tfm, c, duration, depth_test);
		
	}

	void DebugRenderer::Draw(const vec3& start, const vec3& end, const color& c, seconds duration, bool depth_test)
	{
		const vec3 velocity = end - start;
		const auto line_tfm = look_at(start + velocity / 2, start + velocity, vec3{ 0,1,0 }) * mat4 { scale(vec3{ velocity.length() / 2 }) };
		Draw(Mesh::defaults[MeshType::Line], line_tfm, c, duration, depth_test);
	}

	void DebugRenderer::Draw(const sphere& sphere, const color& c, seconds duration, bool depth_test)
	{
		const mat4 tfm = translate(sphere.center) * mat4 { scale(vec3{ sphere.radius * 2 }) };
		Draw(Mesh::defaults[MeshType::Sphere], tfm, c, duration, depth_test);
	}
	void DebugRenderer::Draw(const frustum& frust, const color& c , seconds duration , bool depth_test )
	{
		auto points = frustum_points(frust);
		//Yes I'm too scrub to write some smart code to do this for me.
		Draw(points[0], points[4], c, duration, depth_test);
		Draw(points[1], points[5], c, duration, depth_test);
		Draw(points[2], points[6], c, duration, depth_test);
		Draw(points[3], points[7], c, duration, depth_test);
		Draw(points[0], points[1], c, duration, depth_test);
		Draw(points[1], points[2], c, duration, depth_test);
		Draw(points[2], points[3], c, duration, depth_test);
		Draw(points[3], points[0], c, duration, depth_test);
		Draw(points[4 + 0], points[4 + 1], c, duration, depth_test);
		Draw(points[4 + 1], points[4 + 2], c, duration, depth_test);
		Draw(points[4 + 2], points[4 + 3], c, duration, depth_test);
		Draw(points[4 + 3], points[4 + 0], c, duration, depth_test);

	}

	void DebugRenderer::GraphicsTick()
	{
		for (auto& elem : debug_info)
			elem.display_time += Core::GetRealDT();
		
		debug_info.erase(std::remove_if(debug_info.begin(), debug_info.end(), [](auto& elem) -> bool
		{
			return elem.display_time > elem.duration;
		}), debug_info.end());
	}

	span<const DebugRenderer::DebugInfo> DebugRenderer::GetWorldDebugInfo() const
	{
		return span<const DebugRenderer::DebugInfo>{debug_info};
	}
}