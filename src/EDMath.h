#pragma once


class MPoint;
class MVector;

class EDMath
{
public:
	static MPoint projectOnPlane(const MPoint & point, const MVector & plane_normal
		, const MPoint & ray_origin, const MVector & unit_direction);

	static MVector minimumSkewViewplane(const MVector & ray_direction, const MVector & d);
	static MVector minimumSkewViewplane(const MPoint & camera, const MPoint & p, const MVector & d);

private:
	EDMath();

};

