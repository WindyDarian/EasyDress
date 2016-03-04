// using the lassoTool from Maya 2016 devkit as reference
// this is just an in-progress work
// may not even run
// Windy Darian (Ruoyu Fan)   Mar 3, 2016


#include <maya/MIOStream.h>
#include <math.h>
#include <stdlib.h>
#include <list>
#include <string>

#include <maya/MFnPlugin.h>
#include <maya/MString.h>
#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <maya/MDagPath.h>
 
#include <maya/MItSelectionList.h>

#include <maya/MPxContextCommand.h>
#include <maya/MPxContext.h>
#include <maya/MCursor.h>
#include <maya/MEvent.h>

#include <maya/MGL.h>

#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MItCurveCV.h>
#include <maya/MItSurfaceCV.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>

#include <maya/MUIDrawManager.h>
#include <maya/MItGeometry.h>
#include <maya/MFnTransform.h>

//#ifdef _WIN32
LPCSTR lassoToolCursor = "lassoToolCursor.cur";
//#else
//#include "lassoToolCursor.h"
//#include "lassoToolCursorMask.h"
//
//#define lassoToolCursor_x_hot 1
//#define lassoToolCursor_y_hot 16
//#endif

class coord {
public:
	short h;
	short v;
};

extern "C" int xycompare( coord *p1, coord *p2 );
int xycompare( coord *p1, coord *p2 )
{
	if ( p1->v > p2->v )
		return 1;
	if ( p2->v > p1->v )
		return -1;
	if ( p1->h > p2->h )
		return 1;
	if ( p2->h > p1->h )
		return -1;

	return 0;
}

//////////////////////////////////////////////
// The user Context
//////////////////////////////////////////////

const int initialSize		= 1024;
const int increment			=  256;
const char helpString[]		= "drag mouse to draw strokes";

class easyDressTool : public MPxContext
{
public:
					easyDressTool();
	virtual			~easyDressTool();
	void*			creator();

	virtual void	toolOnSetup( MEvent & event );
	// using Viewport 2.0 of Maya
    virtual MStatus	doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
    virtual MStatus	doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
    virtual MStatus	doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);

private:
	void					append_lasso( short x, short y );
	void					draw_lasso();
	void                    draw_stroke(MHWRender::MUIDrawManager& drawMgr);
	bool					point_in_lasso( coord pt );

	bool					firstDraw;
	coord					min;
	coord					max;
	unsigned				maxSize;
	unsigned				num_points;
	coord*					lasso;
	MGlobal::ListAdjustment	listAdjustment;
	M3dView 				view;
	MCursor					lassoCursor;

};

easyDressTool::easyDressTool()
	: maxSize(0)
	, num_points(0)
	, lasso(NULL)
//#ifdef _WIN32
	, lassoCursor(lassoToolCursor)
//#else
//	, lassoCursor(lassoToolCursor_width,
//			  lassoToolCursor_height,
//			  lassoToolCursor_x_hot,
//			  lassoToolCursor_y_hot,
//			  lassoToolCursor_bits,
//			  lassoToolCursorMask_bits)
//#endif
{
	setTitleString ( "EasyDress Sketch" );
	
	// set the initial state of the cursor
	//setCursor(lassoCursor);

	// Tell the context which XPM to use so the tool can properly
	// be a candidate for the 6th position on the mini-bar.
	//setImage("lassoTool.xpm", MPxContext::kImage1 );
}

easyDressTool::~easyDressTool() {}

void* easyDressTool::creator()
{
	return new easyDressTool;
}

void easyDressTool::toolOnSetup ( MEvent & )
{
	setHelpString( helpString );
}

MStatus easyDressTool::doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
// Set up for overlay drawing, and remember our starting point
{
	// Figure out which modifier keys were pressed, and set up the
	// listAdjustment parameter to reflect what to do with the selected points.
	if (event.isModifierShift() || event.isModifierControl() ) {
		if ( event.isModifierShift() ) {
			if ( event.isModifierControl() ) {
				// both shift and control pressed, merge new selections
				listAdjustment = MGlobal::kAddToList;
			} else {
				// shift only, xor new selections with previous ones
				listAdjustment = MGlobal::kXORWithList;
			}
		} else if ( event.isModifierControl() ) {
			// control only, remove new selections from the previous list
			listAdjustment = MGlobal::kRemoveFromList; 
		}
	} else {
		listAdjustment = MGlobal::kReplaceList;
	}

	// Get the active 3D view.
	//
	view = M3dView::active3dView();

	// Create an array to hold the lasso points. Assume no mem failures
	maxSize = initialSize;
	lasso = (coord*) malloc (sizeof(coord) * maxSize);

	coord start;
	event.getPosition( start.h, start.v );
	num_points = 1;
	lasso[0] = min = max = start;

	firstDraw = true;

	return MS::kSuccess;
}

MStatus easyDressTool::doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
// Add to the growing lasso
{

	if (!firstDraw) {
		//	Redraw the old lasso to clear it.
		draw_stroke(drawMgr);
	} else {
		firstDraw = false;
	}

	coord currentPos;
	event.getPosition( currentPos.h, currentPos.v );
	append_lasso( currentPos.h, currentPos.v );

	////	Draw the new lasso.
	draw_stroke(drawMgr);


	return MS::kSuccess;
}

MStatus easyDressTool::doRelease(MEvent & /*event*/, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
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
	MPoint selectionPoint(0,0,0);

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
		auto world_point = ray_origin + (selectionPoint - ray_origin).length() * ray_direction;
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



	//// We have a non-zero sized lasso.  Close the lasso, and sort
	//// all the points on it.
	////append_lasso(lasso[0].h, lasso[0].v);
	////qsort( &(lasso[0]), num_points, sizeof( coord  ),
	////	(int (*)(const void *, const void *))xycompare);

	//// Save the state of the current selections.  The "selectFromSceen"
	//// below will alter the active list, and we have to be able to put
	//// it back.
	//MGlobal::getActiveSelectionList(incomingList);

	//// As a first approximation to the lasso, select all components with
	//// the bounding box that just contains the lasso.
	//MGlobal::selectFromScreen( min.h, min.v, max.h, max.v,
	//						   MGlobal::kReplaceList );

	//// Get the list of selected items from within the bounding box
	//// and create a iterator for them.
	//MGlobal::getActiveSelectionList(boundingBoxList);

	//// Restore the active selection list to what it was before we
	//// the "selectFromScreen"
	//MGlobal::setActiveSelectionList(incomingList, MGlobal::kReplaceList);

	//// Iterate over the objects within the bounding box, extract the
	//// ones that are within the lasso, and add those to newList.
	//MItSelectionList iter(boundingBoxList);
	//newList.clear();

	//bool	foundEntireObjects = false;
	//bool	foundComponents = false;

	//for ( ; !iter.isDone(); iter.next() ) {
	//	MDagPath	dagPath;
	//	MObject		component;
	//	MPoint		point;
	//	coord		pt;
	//	MObject     singleComponent;

	//	iter.getDagPath( dagPath, component );

	//	if (component.isNull()) {
	//		foundEntireObjects = true;
	//		continue; // not a component
	//	}

	//	foundComponents = true;

//		switch (component.apiType()) {
//		case MFn::kCurveCVComponent:
//			{
//				MItCurveCV curveCVIter( dagPath, component, &stat );
//				for ( ; !curveCVIter.isDone(); curveCVIter.next() ) {
//					point = curveCVIter.position(MSpace::kWorld, &stat );
//					view.worldToView( point, pt.h, pt.v, &stat );
//					if (!stat) {
//						stat.perror("Could not get position");
//						continue;
//					}
//					if ( point_in_lasso( pt ) ) {
//						singleComponent = curveCVIter.cv();
//						newList.add (dagPath, singleComponent);
//					}
//				}
//				break;
//			}
//
//		case MFn::kSurfaceCVComponent:
//			{
//				MItSurfaceCV surfCVIter( dagPath, component, true, &stat );
//				for ( ; !surfCVIter.isDone(); surfCVIter.next() ) {
//					point = surfCVIter.position(MSpace::kWorld, &stat );
//					view.worldToView( point, pt.h, pt.v, &stat );
//					if (!stat) {
//						stat.perror("Could not get position");
//						continue;
//					}
//					if ( point_in_lasso( pt ) ) {
//						singleComponent = surfCVIter.cv();
//						newList.add (dagPath, singleComponent);
//					}
//				}
//				break;
//			}
//
//		case MFn::kMeshVertComponent:
//			{
//				MItMeshVertex vertexIter( dagPath, component, &stat );
//				for ( ; !vertexIter.isDone(); vertexIter.next() ) {
//					point = vertexIter.position(MSpace::kWorld, &stat );
//					view.worldToView( point, pt.h, pt.v, &stat );
//					if (!stat) {
//						stat.perror("Could not get position");
//						continue;
//					}
//					if ( point_in_lasso( pt ) ) {
//						singleComponent = vertexIter.vertex();
//						newList.add (dagPath, singleComponent);
//					}
//				}
//				break;
//			}
//
//		case MFn::kMeshEdgeComponent:
//			{
//				MItMeshEdge edgeIter( dagPath, component, &stat );
//				for ( ; !edgeIter.isDone(); edgeIter.next() ) {
//					point = edgeIter.center(MSpace::kWorld, &stat );
//					view.worldToView( point, pt.h, pt.v, &stat );
//					if (!stat) {
//						stat.perror("Could not get position");
//						continue;
//					}
//					if ( point_in_lasso( pt ) ) {
//						singleComponent = edgeIter.edge();
//						newList.add (dagPath, singleComponent);
//					}
//				}
//				break;
//			}
//
//		case MFn::kMeshPolygonComponent:
//			{
//				MItMeshPolygon polygonIter( dagPath, component, &stat );
//				for ( ; !polygonIter.isDone(); polygonIter.next() ) {
//					point = polygonIter.center(MSpace::kWorld, &stat );
//					view.worldToView( point, pt.h, pt.v, &stat );
//					if (!stat) {
//						stat.perror("Could not get position");
//						continue;
//					}
//					if ( point_in_lasso( pt ) ) {
//						singleComponent = polygonIter.polygon();
//						newList.add (dagPath, singleComponent);
//					}
//				}
//				break;
//			}
//
//		default:
//#ifdef DEBUG
//			cerr << "Selected unsupported type: (" << component.apiType()
//				 << "): " << component.apiTypeStr() << endl;
//#endif /* DEBUG */
//			continue;
//		}
//	}
//
//	// Warn user if zie is trying to select objects rather than components.
//	if (foundEntireObjects && !foundComponents) {
//		MGlobal::displayWarning("lassoTool can only select components, not entire objects.");
//	}
//
//	// Update the selection list as indicated by the modifier keys.
//	MGlobal::selectCommand(newList, listAdjustment);

	// Free the memory that held our lasso points.
	free(lasso);
	lasso = (coord*) 0;
	maxSize = 0;
	num_points = 0;
 
	return MS::kSuccess;
}

void easyDressTool::append_lasso( short x, short y )
{
	int		cy, iy, ix, ydif, yinc, i;
	float	fx, xinc;

	iy = (int)lasso[num_points-1].v;
	ix = (int)lasso[num_points-1].h;
	ydif = abs( y - iy ); 
	if ( ydif == 0 )
		return;

	// Keep track of smallest rectangular area of the screen that
	// completely contains the lasso.
	if ( min.h > x )
		min.h = x;
	if ( max.h < x )
		max.h = x;
	if ( min.v > y )
		min.v = y;
	if ( max.v < y )
		max.v = y;

	if ( ((int)y - iy ) < 0 )
		yinc = -1;
	else
		yinc = 1;

	xinc = (float)((int)x - ix)/(float)ydif;
	fx = (float)ix + xinc;
	cy = iy + yinc;
	for ( i = 0; i < ydif; i++ ) {

		if ( num_points >= maxSize ) {
			// Make the array of lasso points bigger
			maxSize += increment;

			// If realloc() fails, it returns NULL but keeps the old block
			// of memory around, so let's not overwrite the contents of
			// 'lasso' until we know that realloc() worked.
			coord* newLasso = (coord*) realloc (lasso, sizeof(coord) * maxSize);
			if (newLasso == NULL) return;

			lasso = newLasso;
		}

		lasso[num_points].h = (short) fx;
		lasso[num_points].v = (short) cy;
		fx += xinc;
		cy += yinc;
		num_points++;
	}

	return;
}

void easyDressTool::draw_lasso()
{
	//glBegin( GL_LINE_LOOP );
	//for ( unsigned i = 0; i < num_points ; i++ ){
	//	glVertex2i( lasso[i].h, lasso[i].v );
	//}
	//glEnd();

}

void easyDressTool::draw_stroke(MHWRender::MUIDrawManager& drawMgr)
{
	drawMgr.beginDrawable();
	for ( unsigned i = 1; i < num_points ; i++ )
	{
		drawMgr.line2d(MPoint(lasso[i - 1].h, lasso[i - 1].v), MPoint(lasso[i].h, lasso[i].v));
	}
	drawMgr.endDrawable();
}

bool easyDressTool::point_in_lasso( coord pt )
{
	unsigned i, sides;

	for ( i = 0; i < num_points; i++ ) {
		if ( lasso[i].v == pt.v ) {
			while ( (lasso[i].v == pt.v ) && (lasso[i].h < pt.h) )
				i++;
			if ( lasso[i].v != pt.v )
				return( false );
			sides = 0;
			i++;
			while ( lasso[i].v == pt.v  ) {
				i++;
				sides++;
			}
			if ( sides % 2 )
				return( false );
			else
				return( true );
		}
	}
	return( false );
}

//////////////////////////////////////////////
// Command to create contexts
//////////////////////////////////////////////

class lassoContextCmd : public MPxContextCommand
{
public: 
						lassoContextCmd() {};
	virtual MPxContext* makeObj();
	static void*		creator();
};

MPxContext* lassoContextCmd::makeObj()
{
	return new easyDressTool;
}

void* lassoContextCmd::creator()
{
	return new lassoContextCmd;
}

//////////////////////////////////////////////
// plugin initialization
//////////////////////////////////////////////
MStatus initializePlugin( MObject obj )
{
	MStatus		status;
	MFnPlugin	plugin( obj, PLUGIN_COMPANY, "3.0", "Any");
	std::cout << "plugin loaded" << std::endl;
	status = plugin.registerContextCommand( "lassoToolContext",
										    lassoContextCmd::creator);

	if (!status) {
		status.perror("registerContextCommand");
		return status;
	}

    // Register User Interface
    MString cmd_create_ui = R"(source ")" + plugin.loadPath() + R"(/easyDressCreateUI.mel")";
    MString cmd_delete_ui = R"(source ")" + plugin.loadPath() + R"(/easyDressDeleteUI.mel")";
    MGlobal::executeCommand(cmd_create_ui);
    MGlobal::executeCommand(cmd_delete_ui);

	// set the mel scripts to be run when the plugin is loaded / unloaded
	status = plugin.registerUI("easyDressCreateUI", "easyDressDeleteUI");
	if (!status) {
		status.perror("registerUIScripts");
		return status;
	}

	return status;
}

MStatus uninitializePlugin( MObject obj )
{
	MStatus		status;
	MFnPlugin	plugin( obj );

	status = plugin.deregisterContextCommand( "lassoToolContext" );

	return status;
}
