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

#pragma once

#include <maya/MPxContext.h>
#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <maya/MPoint.h>

#include "EDMath.h"

#include <vector>
#include <List>
#include <memory>

class MFnMesh;

class coord {
public:
	short h;
	short v;
	MPoint toMPoint() const;
};

enum EDDrawMode
{
	kDefault,
	kNormal,
	kTangent,
};

struct DrawnCurve
{
	MPoint start;
	MPoint end;
	MString name;

	DrawnCurve(const MPoint & start, const MPoint & end, const MString & name);
};

struct EDAnchor
{
	//TODO: snap to anywhere on current curves
	MPoint point_2D;
	MPoint point_3D;

	EDAnchor(const MPoint & point_2D, const MPoint & point_3D) : point_2D(point_2D), point_3D(point_3D) {}
};

class EasyDressTool : public MPxContext
{
public:
	EasyDressTool();
	virtual			~EasyDressTool();
	void*			creator();

	virtual void toolOnSetup(MEvent & event);
	// using Viewport 2.0 of Maya
	virtual MStatus	doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);


private:
	void append_lasso(short x, short y);
	void update_anchors();
	void draw_stroke(MHWRender::MUIDrawManager& drawMgr);
	bool is_normal(const std::vector<coord> & screen_points, const std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh) const;
	//bool is_tangent() const;
	MString create_curve(std::vector<coord> & screen_points, MFnMesh* selected_mesh, bool start_known, bool end_known, const MPoint& start_point, MPoint& end_point, bool tangent_mode = false, bool normal_mode = false);
	void project_normal(std::vector<coord> & screen_points, std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	void project_tangent(std::vector<coord> & screen_points, std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	void project_contour(std::vector<coord> & screen_points, std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	void project_shell(std::vector<coord> & screen_points, std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	MPoint find_point_nearest_to_mesh(const MFnMesh * selected_mesh, const MPoint & ray_origin, const MVector & ray_direction, const coord & screen_coord, float & ret_height) const;
	void rebuild_kd_2d();
	//void rebuild_kd_3d();
	void rebuild_kd(const MFnMesh * selected_mesh);

	bool drawing_quad = true;
	bool firstDraw;
	coord min;
	coord max;
	unsigned maxSize;

	std::vector<coord> stroke;

	//MGlobal::ListAdjustment	listAdjustment;

	M3dView view;
	double normal_threshold = 0.15;
	int tang_samples = 3;
	EDDrawMode drawMode = EDDrawMode::kDefault;

	EDMath::PointCloud<float> mesh_pts;
	EDMath::PointCloud<float> mesh_pts_2d;

	// kd tree for finding nearest point on mesh
	std::unique_ptr<EDMath::KDTree2D> kd_2d = nullptr;
	std::list<MString> prev_curves;
	std::list<std::pair<MPoint, MPoint>> prev_curve_start_end;
	MString prev_surf;

	std::list<DrawnCurve> drawn_curves;

	std::list<EDAnchor> anchors; 
	EDMath::PointCloud<float> anchors_2d;
	std::unique_ptr<EDMath::KDTree2D> anchors_kd_2d = nullptr;


};
