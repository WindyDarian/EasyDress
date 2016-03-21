#pragma once

#include <maya/MPxContext.h>
#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <List>

class coord {
public:
	short h;
	short v;
	MPoint toMPoint() const;
};


class EasyDressTool : public MPxContext
{
public:
	EasyDressTool();
	virtual			~EasyDressTool();
	void*			creator();

	virtual void	toolOnSetup(MEvent & event);
	// using Viewport 2.0 of Maya
	virtual MStatus	doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
//	virtual bool isNormal(const std::list<MPoint> &stroke, const MFnMesh &selected_mesh) const;
	virtual bool isTangent()const; 

private:
	void					append_lasso(short x, short y);
	void                    draw_stroke(MHWRender::MUIDrawManager& drawMgr);
	bool					firstDraw;
	coord					min;
	coord					max;
	unsigned				maxSize;
	unsigned				num_points;
	coord*					lasso;
	MGlobal::ListAdjustment	listAdjustment;
	M3dView 				view;

};