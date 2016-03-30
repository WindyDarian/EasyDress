#include "EDMath.h"

#include <maya/MPoint.h>

EDMath::EDMath()
{
}

MPoint EDMath::projectOnPlane
	(const MPoint & point, const MVector & plane_normal
		, const MPoint & ray_origin, const MVector & unit_direction)
{
	return ray_origin + ((point - ray_origin) * plane_normal) * unit_direction / (unit_direction * plane_normal);
}

///
//  Create a minimum-skew viewplane
//  d is a direction on the plane;
//  ray_direction and d should be of unit length
//  returns the normal
///
MVector EDMath::minimumSkewViewplane(const MVector & ray_direction, const MVector & d)
{
	// ^ is cross
	return d ^ (ray_direction ^ d);
}

///
//  Create a minimum-skew viewplane
//  d is a direction on the plane, p is a point on the plane;
//  d should be of unit length
//  returns the normal
///
MVector EDMath::minimumSkewViewplane(const MPoint & camera, const MPoint & p, const MVector & d)
{
	auto r = (p - camera).normal();
	return d ^ (r ^ d);
}
