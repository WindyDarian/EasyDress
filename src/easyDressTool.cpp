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

// The main tool for sketching in viewport.
//
// Created: Mar 4, 2016


#include "EasyDressTool.h"

#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MDagPath.h>
#include <maya/MFnTransform.h>
#include <maya/MUIDrawManager.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MPointArray.h>

#include <nanoflann.hpp>
#include "EDMath.h"

#include <string>
#include <list>
#include <vector>

const int initialSize = 1024;
const int increment = 256;
const char helpString[] = "drag mouse to draw strokes";
extern "C" int xycompare(coord *p1, coord *p2);
int xycompare(coord *p1, coord *p2)
{
	if (p1->v > p2->v)
		return 1;
	if (p2->v > p1->v)
		return -1;
	if (p1->h > p2->h)
		return 1;
	if (p2->h > p1->h)
		return -1;

	return 0;
}

EasyDressTool::EasyDressTool()
	: maxSize(0)
	, num_points(0)
	, lasso(NULL)
{
	setTitleString("EasyDress Sketch");
}

EasyDressTool::~EasyDressTool() {}

void* EasyDressTool::creator()
{
	return new EasyDressTool;
}

void EasyDressTool::toolOnSetup(MEvent &)
{
	setHelpString(helpString);
}

MStatus EasyDressTool::doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{


	if (event.isModifierControl())
	{
		drawMode = EDDrawMode::kNormal;
	}
	else if (event.isModifierShift())
	{
		drawMode = EDDrawMode::kTangent;
	}
	else
	{
		drawMode = EDDrawMode::kDefault;
	}

	// Get the active 3D view.
	//
	view = M3dView::active3dView();

	// Create an array to hold the lasso points. Assume no mem failures
	maxSize = initialSize;
	lasso = (coord*)malloc(sizeof(coord) * maxSize);

	coord start;
	event.getPosition(start.h, start.v);
	num_points = 1;
	lasso[0] = min = max = start;

	firstDraw = true;

	return MS::kSuccess;
}

MStatus EasyDressTool::doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
// Add to the growing lasso
{

	if (!firstDraw) {
		//	Redraw the old lasso to clear it.
		draw_stroke(drawMgr);
	}
	else {
		firstDraw = false;
	}

	coord currentPos;
	event.getPosition(currentPos.h, currentPos.v);
	append_lasso(currentPos.h, currentPos.v);

	////	Draw the new lasso.
	draw_stroke(drawMgr);


	return MS::kSuccess;
}

MStatus EasyDressTool::doRelease(MEvent & /*event*/, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
// Selects objects within the lasso
{
	MStatus							stat;
	MSelectionList					incomingList, boundingBoxList, newList;

	if (!firstDraw) {
		// Redraw the lasso to clear it.
		//view.beginXorDrawing(true, true, 1.0f, M3dView::kStippleDashed);
		draw_stroke(drawMgr);
	}
	//view.viewToObjectSpace

	// get selection location
	MGlobal::getActiveSelectionList(incomingList);
	MItSelectionList iter(incomingList);
	MPoint selectionPoint(0, 0, 0);

    MFnMesh * selected_mesh = nullptr;

	for (; !iter.isDone(); iter.next())
	{
		MDagPath dagPath;
		auto stat = iter.getDagPath(dagPath);

        if (stat)
        {

            if (dagPath.hasFn(MFn::kTransform))
            {
                MFnTransform fn(dagPath);
                selectionPoint = fn.getTranslation(MSpace::kWorld);
                dagPath.extendToShape();
            }

            if (dagPath.hasFn(MFn::kMesh))
            {
                MStatus stat_mesh;
                selected_mesh = new MFnMesh(dagPath, &stat_mesh);

                if (stat_mesh)
                {
                    break;
                }
                else
                {
                    selected_mesh = nullptr;
                    continue;
                }
            }
        }
	}

	std::vector<MPoint> world_points;
	world_points.reserve(num_points);
	std::vector<bool> hit_list;
	hit_list.reserve(num_points);
	std::vector<std::pair<MPoint, MVector>> rays;
	rays.reserve(num_points);

	// TODO: rebuild kd-tree only when view is changed
	rebuild_kd(selected_mesh);

	unsigned hit_count = 0;
	// calculate points in world space
	for (unsigned i = 0; i < num_points; i++)
	{
		MPoint ray_origin;
		MVector ray_direction;

        view.viewToWorld(lasso[i].h, lasso[i].v, ray_origin, ray_direction);
		rays.push_back(std::pair<MPoint, MVector>(ray_origin, ray_direction));
        MPoint world_point;
        bool hit = false;

        if (selected_mesh)
        {
            //MIntArray faceids;
            MFloatPoint hit_point;
            float hit_param;
            int hit_face;
            int hit_tri;
            float hit_bary1;
            float hit_bary2;

            bool intersected = selected_mesh->closestIntersection(ray_origin,
                ray_direction,
                nullptr,
                nullptr,
                true,
                MSpace::kWorld,
                10000, // maxParam
                false, // testBothDirections
                nullptr,
                hit_point,
                &hit_param,
                &hit_face,
                &hit_tri,
                &hit_bary1,
                &hit_bary2
                );

            if (intersected)
            {
                world_point = hit_point;
                hit = true;
				hit_count++;
            }
        }

        if (!hit)
        {
			// project it to the plane where all points are on
            // * is dot product
            //world_point = ray_origin + (selectionPoint - ray_origin).length() * ray_direction / ((selectionPoint - ray_origin).normal() * ray_direction);
            //a          uto world_point = ray_origin + (selectionPoint - ray_origin).length() * ray_direction;
			world_point = EDMath::projectOnPlane(selectionPoint, -ray_direction, ray_origin, ray_direction);
        }
		world_points.push_back(world_point);
		hit_list.push_back(hit);
	}

	if (world_points.size() > 1)
	{
		if (hit_count == 0)
		{
			project_contour(world_points, hit_list, selected_mesh, rays);
			// classify this curve as shell contour
			setHelpString("Classified: Shell Contour!");

			// TODO: SHAPE MATCHING!
		}
		else if ((is_normal(world_points, hit_list, selected_mesh) || drawMode == kNormal) && (hit_list[0] || hit_list[num_points-1]))
		{
			project_normal(world_points, hit_list, selected_mesh, rays);
			setHelpString("Classified: Normal!");
		}
		//else if (is_tangent())
		else if (drawMode == kTangent) 
		{
			// TODO: actually use is_tangent the same time as force tangent
			project_tangent(world_points, hit_list, selected_mesh, rays);
            setHelpString("Classified: Tangent Plane!");
		}
		else
		{
			project_shell(world_points, hit_list, selected_mesh, rays);
			setHelpString("Classified: Shell Projection!");
		}


		// create the curve
		std::string curve_command;
		curve_command.reserve(world_points.size() * 40 + 40);
		curve_command.append("proc string __ed_draw_curve() { \n");
		curve_command.append("string $slct[]=`ls- sl`;\n");
		curve_command.append("string $cv = `curve");
		for (auto & p : world_points)
		{
			curve_command.append(" -p ");
			curve_command.append(std::to_string(p.x));
			curve_command.append(" ");
			curve_command.append(std::to_string(p.y));
			curve_command.append(" ");
			curve_command.append(std::to_string(p.z));
		}
		curve_command.append("`;\n");
		// smooth the curve
		curve_command.append("rebuildCurve -ch 1 -rpo 1 -rt 0 -end 1 -kr 0 -kcp 0 -kep 1 -kt 0 -s 8 -d 3 -tol 0.01 $cv; \n");
		curve_command.append("select $slct; \n");
		curve_command.append("return $cv; \n } \n");
		curve_command.append("__ed_draw_curve();");

		MString curve_name;
		MGlobal::executeCommand(MString(curve_command.c_str()), curve_name);
		curves.push_back(curve_name);
		if (curves.size() == 4){
			std::string surface_command;
			surface_command.reserve(world_points.size() * 40 + 40);
			surface_command.append("select -r ");
			std::list<std::string> curve_names;
			while (!curves.empty())
			{
				curve_names.push_back(curves.front().asChar());
				surface_command.append(curves.front().asChar());
				curves.pop_front();
				surface_command.append(" ");
			}
			surface_command.append(";\n");
			surface_command.append("boundary -ch 1 -or 0 -ep 0 -rn 0 -po 0 -ept 0.01 ");
			while (!curve_names.empty())
			{
				surface_command.append(" ");
				surface_command.append("\""+curve_names.front()+"\"");
				curve_names.pop_front();
			}
			surface_command.append(";\n");
			MGlobal::executeCommand(MString(surface_command.c_str()));
		}
	}
	free(lasso);
	lasso = (coord*)0;
	maxSize = 0;
	num_points = 0;

    if (selected_mesh)
        delete selected_mesh;
    selected_mesh = nullptr;

	return MS::kSuccess;
}

bool EasyDressTool::is_normal(const std::vector<MPoint> & world_points, const std::vector<bool> & hit_list,
	    const MFnMesh * selected_mesh) const
{
	if (!selected_mesh)
	{
		return false;
	}

	if (hit_list[0])
	{
		// if starting point is normal
		//unsigned sample_num = 0;
		MPoint p0 = lasso[0].toMPoint();
		MVector current_tang;
		for (int i = 1; i < tang_samples; i++)
		{
			if (i >= num_points) break;
			current_tang += lasso[i].toMPoint() - p0;
			//sample_num++;
		}
		current_tang.normalize();

		MPoint closest_point;
		MVector surface_normal;
		selected_mesh->getClosestPointAndNormal(world_points[0], closest_point, surface_normal, MSpace::kWorld);
		surface_normal.normalize();
		MPoint point_plus_normal = closest_point + surface_normal;

		coord cp0, cp1;
		view.worldToView(closest_point, cp0.h, cp0.v);
		view.worldToView(point_plus_normal, cp1.h, cp1.v);
		MVector norm_proj = cp1.toMPoint() - cp0.toMPoint();
		norm_proj.normalize();

		// * is dot... strange Maya...
        auto v = 1.0 - (current_tang * norm_proj);
		return v < normal_threshold;
	}

	// todo: last_hit

}

bool EasyDressTool::is_tangent()const
{
	// FIXME: curvature has problems.
	// not very using this function since I am just using SHIFT to force tangent
	double menger_curvature = 0;
    unsigned valid_points = 0;
	for (int i = 0; i < num_points-2; i++){
        MPoint x = lasso[i].toMPoint();
		MPoint y = lasso[i + 1].toMPoint();
		MPoint z = lasso[i + 2].toMPoint();
		MVector xy = x - y;
		MVector zy = z - y;
		MVector zx = z - x;
		double area = (xy^zy).length();

        if (xy.length() == 0 && zy.length() == 0 && zx.length() == 0){
            continue;
        }

		if (xy.length() != 0 && zy.length() != 0 && zx.length() != 0){
			menger_curvature += 4 * area / (xy.length()*zy.length()*zx.length());
		}
        valid_points += 1;
	}

    if (valid_points == 0) return false;

	menger_curvature /= valid_points;
	if (menger_curvature <= 0.2){
		return true;
	}
	return false;
}

void EasyDressTool::project_normal(std::vector<MPoint>& world_points, const std::vector<bool>& hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays)
{
	if (!selected_mesh)
	{
		return;
	}

	if (hit_list[0])
	{

		MPoint closest_point;
		MVector surface_normal;
		selected_mesh->getClosestPointAndNormal(world_points[0], closest_point, surface_normal, MSpace::kWorld);
		surface_normal.normalize();

		auto normal = EDMath::minimumSkewViewplane(rays[0].second, surface_normal);
		auto point = world_points[0];
		auto point_num = rays.size();

		for (int i = 0; i < point_num; i++)
		{
			world_points[i] = EDMath::projectOnPlane(point, normal, rays[i].first, rays[i].second);
		}
	}
	// todo: last hit
}
///
// Find a point on a camera ray that is nearest to the mesh
///
MPoint EasyDressTool::find_point_nearest_to_mesh(const MFnMesh * selected_mesh, const MPoint & ray_origin, const MVector & ray_direction, const coord & screen_coord, float & ret_height) const
{
	if (!selected_mesh)
	{
		return ray_origin;
	}

	float pt[] = { screen_coord.h , screen_coord.v, 0 };
	size_t out_index = 0;
	float out_dist_squared = 0;
	kd_2d->knnSearch(pt, 1, &out_index, &out_dist_squared);

	// nearest point (I am just using vertex for now) on the mesh
	auto temp = mesh_pts.pts[out_index];
	MPoint p_on_mesh(temp.x, temp.y, temp.z);

	auto dist = (ray_direction * (p_on_mesh - ray_origin));
	if (dist < 0)
	{
		return ray_origin;
	}

	auto p_on_ray = ray_origin + dist * ray_direction;

	ret_height = (p_on_ray - p_on_mesh).length();

	return p_on_ray;
}

void EasyDressTool::project_contour(std::vector<MPoint>& world_points, const std::vector<bool>& hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>>& rays)
{
	if (!selected_mesh || !kd_2d || world_points.size() < 2)
	{
		return;
	}

	// TODO: with shape matching
	// TODO: find nearest point on mesh, not vertex

	auto length = rays.size();
	float dummy;
	auto s0 = find_point_nearest_to_mesh(selected_mesh, rays[0].first, rays[0].second, lasso[0], dummy);
	auto sn = find_point_nearest_to_mesh(selected_mesh, rays[length - 1].first, rays[length - 1].second, lasso[length - 1], dummy);

	if (s0.isEquivalent(sn)) return;

	auto d = (sn - s0).normal();
	auto normal = EDMath::minimumSkewViewplane(rays[0].second, d);

	for (int i = 0; i < length; i++)
	{
		world_points[i] = EDMath::projectOnPlane(s0, normal, rays[i].first, rays[i].second);
	}

}

//MPoint interpolate_point(const MPoint& p, const MPoint& p_start, const MPoint& p_end, const MPoint& w_start, const MPoint& w_end)
//{
//	auto w1 = (p - p_start).length();
//	auto w2 = (p - p_end).length();
//
//	return (w2 * w_start + w1 * w_end) / (w1 + w2);
//}

double interpolate_height(const MPoint& p, const MPoint& p_start, const MPoint& p_end, double h_start, double h_end)
{
	auto w1 = (p - p_start).length();
	auto w2 = (p - p_end).length();

	return (w2 * h_start + w1 * h_end) / (w1 + w2);

}

///                 
// Shell Projection
///
void EasyDressTool::project_shell(std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays)
{
	if (!selected_mesh || !kd_2d || world_points.size() < 2)
	{
		return;
	}

	// TODO: snaping to a known height and do interpolation

	auto length = rays.size();
	float start_height = 0, end_height = 0;
	//MPoint s0 = world_points[0];
	//MPoint sn = world_points[length - 1];
	//int iter_start = 0, iter_end = length;

	if (!hit_list[0])
	{
		world_points[0] = find_point_nearest_to_mesh(selected_mesh, rays[0].first, rays[0].second, lasso[0], start_height);
	}

	if (!hit_list[length - 1])
	{
		world_points[length - 1] = find_point_nearest_to_mesh(selected_mesh, rays[length - 1].first, rays[length - 1].second, lasso[length - 1], end_height);
	}

	int first_miss = -1, last_miss = -1;
	for (size_t i = 1; i <= length - 2; i++)
	{
		if (!hit_list[i])
		{
			if (first_miss == -1)
				first_miss = i;
			last_miss = i;
		}
		else
		{
			// TODO: known heights other than h0 and hn.
			auto h = interpolate_height(rays[i].first, rays[0].first, rays[length - 1].first, start_height, end_height);
			world_points[i] = (-rays[i].second) * h + world_points[i];

			if (first_miss != -1 && last_miss != -1)
			{
                auto plane_normal = EDMath::minimumSkewViewplane(rays[first_miss - 1].second
                    , world_points[last_miss + 1] - world_points[first_miss - 1]);
				for (size_t j = first_miss; j <= last_miss; j++)
				{
                    world_points[j] = EDMath::projectOnPlane(world_points[first_miss - 1], plane_normal, rays[j].first, rays[j].second);
                    
					//world_points[j] = interpolate_point(rays[j].first, rays[first_miss - 1].first, rays[last_miss + 1].first
					//	, world_points[first_miss - 1], world_points[last_miss + 1]);
				}
			}
			first_miss = -1;
			last_miss = -1;
		}
	}
	if (first_miss != -1 && last_miss != -1)
	{
        auto plane_normal = EDMath::minimumSkewViewplane(rays[first_miss - 1].second
            , world_points[last_miss + 1] - world_points[first_miss - 1]);
        for (size_t j = first_miss; j <= last_miss; j++)
        {
            world_points[j] = EDMath::projectOnPlane(world_points[first_miss - 1], plane_normal, rays[j].first, rays[j].second);

            //world_points[j] = interpolate_point(rays[j].first, rays[first_miss - 1].first, rays[last_miss + 1].first
            //	, world_points[first_miss - 1], world_points[last_miss + 1]);
        }
	}
}
// tangent projection
void EasyDressTool::project_tangent(std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays)
{
	if (!selected_mesh || !kd_2d || world_points.size() < 2){
		return;
	}
	auto length = rays.size();
	//determine the height of the tangent plane and the middle point on that plane
	//assume the average height is the height of the middle point
	float h = 0.0;
	int mid_index = int(length / 2);
	MPoint nearest_point = find_point_nearest_to_mesh(selected_mesh, rays[mid_index].first, rays[mid_index].second, lasso[mid_index], h);
	MPoint middle_point = (-rays[mid_index].second)*h + world_points[mid_index];
 
	//project each stroke points on the base layer and get each normal
	MVector normal;
	MPoint closest_point;
	MVector sum_normal = MVector(0.0, 0.0, 0.0);

	for (int i = 0; i < length; i++){
		selected_mesh->getClosestPointAndNormal(world_points[i], closest_point, normal, MSpace::kWorld);
		sum_normal += normal;
	}
	//calculate the average normal as the tangent plane normal
	sum_normal = MVector(sum_normal.x / length, sum_normal.y / length, sum_normal.z / length);
	MVector plane_normal = sum_normal.normal();
	//project all the point on to the tangent plane
	for (int i = 0; i < length; i++){
		world_points[i] = (-rays[i].second) * h + world_points[i];
		world_points[i] = EDMath::projectOnPlane(middle_point, plane_normal, rays[i].first, rays[i].second);
	}
}
void EasyDressTool::rebuild_kd(const MFnMesh * selected_mesh)
{
	mesh_pts.clear();
	mesh_pts_2d.clear();
	if (!selected_mesh)
	{
		kd_2d = nullptr;
		return;
	}
	MPointArray pts_array;
	selected_mesh->getPoints(pts_array, MSpace::kWorld);

	auto length = pts_array.length();

	mesh_pts.pts.resize(length);
	for (int i = 0; i < length; i++)
	{
		mesh_pts.pts[i].x = pts_array[i].x;
		mesh_pts.pts[i].y = pts_array[i].y;
		mesh_pts.pts[i].z = pts_array[i].z;
	}

	mesh_pts_2d.pts.resize(length);
	for (int i = 0; i < length; i++)
	{
		short x, y;
		view.worldToView(pts_array[i], x, y);
		mesh_pts_2d.pts[i].x = x;
		mesh_pts_2d.pts[i].y = y;
		mesh_pts_2d.pts[i].z = 0;
	}

	kd_2d.reset(new EDMath::KDTree2D(2 /*dim*/, mesh_pts_2d, nanoflann::KDTreeSingleIndexAdaptorParams(10)));
	kd_2d->buildIndex();

}

void EasyDressTool::append_lasso(short x, short y)
{
	int		cy, iy, ix, ydif, yinc, i;
	float	fx, xinc;

	iy = (int)lasso[num_points - 1].v;
	ix = (int)lasso[num_points - 1].h;
	ydif = abs(y - iy);
	if (ydif == 0)
		return;

	// Keep track of smallest rectangular area of the screen that
	// completely contains the lasso.
	if (min.h > x)
		min.h = x;
	if (max.h < x)
		max.h = x;
	if (min.v > y)
		min.v = y;
	if (max.v < y)
		max.v = y;

	if (((int)y - iy) < 0)
		yinc = -1;
	else
		yinc = 1;

	xinc = (float)((int)x - ix) / (float)ydif;
	fx = (float)ix + xinc;
	cy = iy + yinc;
	for (i = 0; i < ydif; i++) {

		if (num_points >= maxSize) {
			// Make the array of lasso points bigger
			maxSize += increment;

			// If realloc() fails, it returns NULL but keeps the old block
			// of memory around, so let's not overwrite the contents of
			// 'lasso' until we know that realloc() worked.
			coord* newLasso = (coord*)realloc(lasso, sizeof(coord) * maxSize);
			if (newLasso == NULL) return;

			lasso = newLasso;
		}

		lasso[num_points].h = (short)fx;
		lasso[num_points].v = (short)cy;
		fx += xinc;
		cy += yinc;
		num_points++;
	}

	return;
}

void EasyDressTool::draw_stroke(MHWRender::MUIDrawManager& drawMgr)
{
	drawMgr.beginDrawable();
	for (unsigned i = 1; i < num_points; i++)
	{
		drawMgr.line2d(MPoint(lasso[i - 1].h, lasso[i - 1].v), MPoint(lasso[i].h, lasso[i].v));
	}
	drawMgr.endDrawable();
}

MPoint coord::toMPoint() const
{
	return MPoint(static_cast<double>(h), static_cast<double>(v));
}
