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
	void draw_stroke(MHWRender::MUIDrawManager& drawMgr);
	bool is_normal(const std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh) const;
	bool is_tangent() const;
	void project_normal(std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	void project_tangent(std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	void project_contour(std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	void project_shell(std::vector<MPoint> & world_points, const std::vector<bool> & hit_list, const MFnMesh * selected_mesh, std::vector<std::pair<MPoint, MVector>> & rays);
	MPoint find_point_nearest_to_mesh(const MFnMesh * selected_mesh, const MPoint & ray_origin, const MVector & ray_direction, const coord & screen_coord, float & ret_height) const;
	void rebuild_kd_2d();
	//void rebuild_kd_3d();
	void rebuild_kd(const MFnMesh * selected_mesh);

	bool drawing_quad = true;
	bool firstDraw;
	coord min;
	coord max;
	unsigned maxSize;
	unsigned num_points;
	coord* lasso;
	//std::list<coord> points_2d;
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
};
