#pragma once

#include <maya/MPxContext.h>
#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <vector>

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
};