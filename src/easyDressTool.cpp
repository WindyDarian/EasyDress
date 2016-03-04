// The main tool for sketching in viewport.
// 
// Created: Mar 4, 2016

#include <string>
#include <list>

#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MDagPath.h>
#include <maya/MFnTransform.h>
#include <maya/MUIDrawManager.h>

#include "EasyDressTool.h"

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
	// Figure out which modifier keys were pressed, and set up the
	// listAdjustment parameter to reflect what to do with the selected points.
	if (event.isModifierShift() || event.isModifierControl()) {
		if (event.isModifierShift()) {
			if (event.isModifierControl()) {
				// both shift and control pressed, merge new selections
				listAdjustment = MGlobal::kAddToList;
			}
			else {
				// shift only, xor new selections with previous ones
				listAdjustment = MGlobal::kXORWithList;
			}
		}
		else if (event.isModifierControl()) {
			// control only, remove new selections from the previous list
			listAdjustment = MGlobal::kRemoveFromList;
		}
	}
	else {
		listAdjustment = MGlobal::kReplaceList;
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

	for (; !iter.isDone(); iter.next())
	{
		MDagPath dagPath;
		MObject component;

		iter.getDagPath(dagPath, component);
		//if (obj.hasFn(type)) {
		//	objects.append(obj);
		//}
		if (component.hasFn(MFn::kTransform)) {

			MFnTransform fn(component);
			selectionPoint = fn.getTranslation(MSpace::kWorld);
			break;
		}
	}



	std::list<MPoint> world_points;
	// calculate points in world space
	for (unsigned i = 0; i < num_points; i++)
	{
		MPoint ray_origin;
		MVector ray_direction;

		view.viewToWorld(lasso[i].h, lasso[i].v, ray_origin, ray_direction);
		// * is dot product
		auto world_point = ray_origin + (selectionPoint - ray_origin).length() * ray_direction / ((selectionPoint - ray_origin).normal() * ray_direction);
		//auto world_point = ray_origin + (selectionPoint - ray_origin).length() * ray_direction;
		world_points.push_back(world_point);
	}

	if (world_points.size() > 1)
	{
		std::string curveCommand;
		curveCommand.reserve(world_points.size() * 40 + 7);
		curveCommand.append("curve");
		for (auto & p : world_points)
		{
			curveCommand.append(" -p ");
			curveCommand.append(std::to_string(p.x));
			curveCommand.append(" ");
			curveCommand.append(std::to_string(p.y));
			curveCommand.append(" ");
			curveCommand.append(std::to_string(p.z));
		}
		curveCommand.append(";");
		MGlobal::executeCommand(MString(curveCommand.c_str()));
	}

	free(lasso);
	lasso = (coord*)0;
	maxSize = 0;
	num_points = 0;

	return MS::kSuccess;
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
