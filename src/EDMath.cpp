// =============================================================================
//
// EasyDress: a 3D sketching plugin for Maya
// Copyright (C) 2016  Ruoyu Fan (Windy Darian), Yimeng Xu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// =============================================================================

// Created: Mar 29, 2016

#include "EDMath.h"

#include <maya/MPoint.h>


MPoint EDMath::projectOnPlane
	(const MPoint & point, const MVector & plane_normal
		, const MPoint & ray_origin, const MVector & unit_direction)
{
	return ray_origin + ((point - ray_origin) * plane_normal) * unit_direction
        / (unit_direction * plane_normal);
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
