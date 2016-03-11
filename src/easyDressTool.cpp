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
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>


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

                //// We have a mesh so create a vertex and polygon table
                //// for this object.
                ////
                //MFnMesh fnMesh(dagPath);
                //int vtxCount = fnMesh.numVertices();
                //int polygonCount = fnMesh.numPolygons();
                //// we do not need this call anymore, we have the shape.
                //// dagPath.extendToShape();
                //MString name = dagPath.fullPathName();
                //objectNames->append(name);
                //objectNodeNamesArray.append(fnMesh.name());

                //vertexCounts.append(vtxCount);
                //polygonCounts.append(polygonCount);

                //objectCount++;
            }
        }
		//if (obj.hasFn(type)) {
		//	objects.append(obj);
		//}
		//if (component.hasFn(MFn::kTransform)) {

		//	MFnTransform fn(component);
		//	selectionPoint = fn.getTranslation(MSpace::kWorld);
		//	break;
		//}

	}
	


	std::list<MPoint> world_points;
	// calculate points in world space
	for (unsigned i = 0; i < num_points; i++)
	{
		MPoint ray_origin;
		MVector ray_direction;

        view.viewToWorld(lasso[i].h, lasso[i].v, ray_origin, ray_direction);

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
            }
        }

        if (!hit)
        {
            // * is dot product
            world_point = ray_origin + (selectionPoint - ray_origin).length() * ray_direction / ((selectionPoint - ray_origin).normal() * ray_direction);
            //auto world_point = ray_origin + (selectionPoint - ray_origin).length() * ray_direction;
        }
		world_points.push_back(world_point);
	}

	if (world_points.size() > 1)
	{
		std::string curve_command;
		curve_command.reserve(world_points.size() * 40 + 7);
		curve_command.append("curve");
		for (auto & p : world_points)
		{
			curve_command.append(" -p ");
			curve_command.append(std::to_string(p.x));
			curve_command.append(" ");
			curve_command.append(std::to_string(p.y));
			curve_command.append(" ");
			curve_command.append(std::to_string(p.z));
		}
		curve_command.append(";");
		MGlobal::executeCommand(MString(curve_command.c_str()));
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
