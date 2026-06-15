/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Worm class - AI
// Created 13/3/03
// Jason Boettcher
// Dark Charlie
// Albert Zeyer

// TODO: cleanup!!!


#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

#include <cassert>
#include <set>

#include "CodeAttributes.h"
#include "LieroX.h"
#include "CGameScript.h"
#include "MathLib.h"
#include "CClient.h"
#include "CBonus.h"
#include "GfxPrimitives.h"
#include "StringUtils.h"
#include "game/CWorm.h"
#include "AuxLib.h"
#include "Timer.h"
#include "ProfileSystem.h"
#include "CWormBot.h"
#include "Debug.h"
#include "CGameMode.h"
#include "CHideAndSeek.h"
#include "FlagInfo.h"
#include "ProjectileDesc.h"
#include "WeaponDesc.h"
#include "Mutex.h"
#include "game/Game.h"
#include "gusanos/weapon.h"
#include "game/Mod.h"
#include "level/FastTraceLine.h"
#include "CClientNetEngine.h"


// used by searchpath algo
static const unsigned short wormsize = 7;


/*
===============================

    Artificial Intelligence

===============================
*/


// AI states
enum {
    AI_THINK,
    //AI_FINDTARGET,
    AI_MOVINGTOTARGET,
    AI_AIMING,
    AI_SHOOTING
};

// Target types
enum {
    AIT_NONE,
    AIT_WORM,
    AIT_BONUS,
    AIT_POSITION
};

// AI Game Type
enum GAM_AI_TYPE
{
	GAM_RIFLES	= 0,
	GAM_100LT	= 1,
	GAM_MORTARS	= 2,
	GAM_OTHER	= 3
};


struct NEW_ai_node_t {
	float fX, fY;
	NEW_ai_node_t *psPrev, *psNext;
	NEW_ai_node_t() : fX(0), fY(0), psPrev(NULL), psNext(NULL) {}
};

NEW_ai_node_t* get_last_ai_node(NEW_ai_node_t* n);
void delete_ai_nodes(NEW_ai_node_t* start);
void delete_ai_nodes(NEW_ai_node_t* start, NEW_ai_node_t* end);
float get_ai_nodes_length(NEW_ai_node_t* start);
// this do the same as the fct above except that it don't do the sqrt
float get_ai_nodes_length2(NEW_ai_node_t* start);



// returns the biggest possible free rectangle with the given point inside
// (not in every case, but in most it is the biggest; but it is ensured that the given rect is at least free)
// the return-value of type SquareMatrix consists of the top-left and upper-right pixel
// WARNING: if the given point is not in the map, the returned
// area is also not in the map (but it will handle it correctly)
SquareMatrix<int> getMaxFreeArea(VectorD2<int> p, uchar checkflag) {
	uint map_w = game.gameMap()->GetWidth();
	uint map_h = game.gameMap()->GetHeight();
	uchar** pxflags = game.gameMap()->material->line;
	boost::array<Material,256>& materials = game.gameMap()->materialArray();

	SquareMatrix<int> ret;
	ret.v1 = p; ret.v2 = p;

	// just return if we are outside
	if(p.x < 0 || (uint)p.x >= map_w
	|| p.y < 0 || (uint)p.y >= map_h)
		return ret;

	enum { GO_RIGHT=1, GO_DOWN=2, GO_LEFT=4, GO_UP=8 }; short dir;
	unsigned short col;
	int x=0, y=0;

	// loop over all directions until there is some obstacle
	col = 0; dir = 1;
	while(true) {
		if(col == 0xF) // got we collisions in all directions?
			break;

		// change direction
		do {
			dir = dir << 1;
			if(dir > 8) dir = 1;
		} while(col & dir);

		// set start pos
		switch(dir) {
		case GO_RIGHT: x=ret.v2.x+1; y=ret.v1.y; break;
		case GO_DOWN: x=ret.v1.x; y=ret.v2.y+1; break;
		case GO_LEFT: x=ret.v1.x-1; y=ret.v1.y; break;
		case GO_UP: x=ret.v1.x; y=ret.v1.y-1; break;
		}

		// check if still inside the map (than nothing bad can happen)
		if(x < 0 || (uint)x >= map_w
		|| y < 0 || (uint)y >= map_h) {
			col |= dir;
			continue;
		}

		// loop over all pxflags of the aligned line and check for an obstacle
		while(true) {
			// break if ready
			if(dir == GO_RIGHT || dir == GO_LEFT) {
				if(y > ret.v2.y) break;
			} else // GO_UP / GO_DOWN
				if(x > ret.v2.x) break;

			// is there some obstacle?
			if(materials[pxflags[y][x]].toLxFlags() & checkflag) {
				col |= dir;
				break;
			}

			// inc the pos (trace the aligned line)
			switch(dir) {
			case GO_RIGHT: case GO_LEFT: y++; break;
			case GO_DOWN: case GO_UP: x++; break;
			}
		}

		if(!(col & dir)) {
			// simple inc 1 pixel in the checked direction
			switch(dir) {
			case GO_RIGHT: ret.v2.x++; break;
			case GO_DOWN: ret.v2.y++; break;
			case GO_LEFT: ret.v1.x--; break;
			case GO_UP: ret.v1.y--; break;
			}
		}

	} // loop over directions

	// cut the area if outer space...
	if(ret.v1.x < 0) ret.v1.x = 0;
	if(ret.v1.y < 0) ret.v1.y = 0;
	if((uint)ret.v2.x >= map_w) ret.v2.x = map_w-1;
	if((uint)ret.v2.y >= map_h) ret.v2.y = map_h-1;

	return ret;
}




NEW_ai_node_t* createNewAiNode(float x, float y, NEW_ai_node_t* next = NULL, NEW_ai_node_t* prev = NULL) {
	NEW_ai_node_t* tmp = new NEW_ai_node_t;
	tmp->fX = x; tmp->fY = y;
	tmp->psNext = next; tmp->psPrev = prev;
	return tmp;
}

NEW_ai_node_t* createNewAiNode(NEW_ai_node_t* base) {
	if(!base) return NULL;
	NEW_ai_node_t* tmp = new NEW_ai_node_t;
	tmp->fX = base->fX; tmp->fY = base->fY;
	tmp->psNext = base->psNext; tmp->psPrev = base->psPrev;
	return tmp;
}

NEW_ai_node_t* createNewAiNode(const VectorD2<int>& p) {
	return createNewAiNode((float)p.x, (float)p.y);
}

// returns true, if the given line is free or not
// these function will either go parallel to the x-axe or parallel to the y-axe
// (depends on which of them is the absolute greatest)
// HINT: don't lock the flags here (it's done in the caller)
INLINE bool simpleTraceLine(VectorD2<int> start, VectorD2<int> dist, uchar checkflag) {
	boost::array<Material,256>& materials = game.gameMap()->materialArray();
	unsigned char** pxflags = game.gameMap()->material->line;
	if (!pxflags)  {  // The map has been probably shut down
		warnings << "simpleTraceLine with pxflags==NULL" << endl;
		return false;
	}
	uint map_w = game.gameMap()->GetWidth();
	uint map_h = game.gameMap()->GetHeight();

	if(abs(dist.x) >= abs(dist.y)) {
		if(dist.x < 0) { // avoid anoying checks
			start.x += dist.x;
			dist.x = -dist.x;
		}
		if(start.x < 0 || (uint)(start.x + dist.x) >= map_w || start.y < 0 || (uint)start.y >= map_h)
			return false;
		for(int x = 0; x <= dist.x; x++) {
			if(materials[pxflags[start.y][start.x + x]].toLxFlags() & checkflag)
				return false;
		}
	} else { // y is greater
		if(dist.y < 0) { // avoid anoying checks
			start.y += dist.y;
			dist.y = -dist.y;
		}
		if(start.y < 0 || (uint)(start.y + dist.y) >= map_h || start.x < 0 || (uint)start.x >= map_w)
			return false;
		for(int y = 0; y <= dist.y; y++) {
			if(materials[pxflags[start.y+y][start.x]].toLxFlags() & checkflag)
				return false;
		}
	}
	return true;
}


/*
this class do the whole pathfinding (idea by AZ)
you can use the findPath-function directly,
or you can use the function StartThreadSearch, which
start the search in an own thread; you can ask
for the state with IsReady
*/
class searchpath_base {
public:

// this will define, if we should break the search on the first result
// (we ensure in forEachChecklistItem, that it is not that bad)
#define		BREAK_ON_FIRST	1

	// this is one node in the map
	class area_item {
	public:

		// needed for area_set (for area search function getArea(p))
		struct area_v1__less {
			// this is a well-defined transitive ordering after the v1-vector of the matrix
			bool operator()(const area_item* a, const area_item* b) const {
				if(!a || !b) return a < b; // this should never happen
				return a->area.v1 < b->area.v1;
			}
		};

		// needed for area_set (for area search function getBestArea)
		struct expected_min_total_dist__less {
			// this is a well-defined transitive ordering after the v1-vector of the matrix
			bool operator()(const area_item* a, const area_item* b) const {
				if(!a || !b) return a < b; // this should never happen
				double dista = a->expected_min_total_dist();
				double distb = b->expected_min_total_dist();
				// In some rare cases (depending on optimisation and hardware, (a<b)&&(b<a) is true here.
				// To make it a valid order, we have to make the additional check.
				if(fabs(dista - distb) < 0.0001) return false;
				return dista < distb;
			}
		};

		searchpath_base* const base;

		// this will save the state, if we still have to check a specific end
		// at the rectangle or not
		short checklistRows;
		short checklistRowStart;
		short checklistRowHeight;
		short checklistCols;
		short checklistColStart;
		short checklistColWidth;

		const SquareMatrix<int> area;
		area_item* lastArea; // the area where we came from
		double dist_from_source; // distance from startpoint

		void initChecklists() {
			VectorD2<int> size = area.v2 - area.v1;
			// ensure here, that the starts are not 0
			checklistCols = (size.x-3) / checklistColWidth + 1;
			checklistRows = (size.y-3) / checklistRowHeight + 1;
			checklistColStart = (size.x - (checklistCols-1)*checklistColWidth) / 2;
			checklistRowStart = (size.y - (checklistRows-1)*checklistRowHeight) / 2;
		}

		// expected minimum total distance from startpoint to destination
		double expected_min_total_dist() const {
			return dist_from_source + (area.getCenter() - base->target).GetLength();
		}

		double expected_min_total_dist(area_item* lastArea) const {
			return lastArea->dist_from_source + getDistToArea(lastArea) + (area.getCenter() - base->target).GetLength();
		}

		void setLastArea(area_item* theValue) {
			lastArea = theValue;
			if(lastArea == NULL)
				dist_from_source = 0;
			else
				dist_from_source = lastArea->dist_from_source + getDistToArea(lastArea);
		}

		VectorD2<int> getConnectorPointForArea(area_item* otherArea) const {
			if(otherArea == NULL) return area.getCenter();

			SquareMatrix<int> intersection = area.getInsersectionWithArea(otherArea->area);
			return intersection.getCenter();
			/*
			VectorD2<int> otherCenter = otherArea->area.getCenter();
			if(area.v1.x <= otherCenter.x && otherCenter.x <= area.v2.x)
				return VectorD2<int>(otherCenter.x, area.getCenter().y);
			else
				return VectorD2<int>(area.getCenter().x, otherCenter.y); */
		}

		double getDistToArea(area_item* otherArea) const {
			VectorD2<int> conPoint = getConnectorPointForArea(otherArea);
			return
				(otherArea->area.getCenter() - conPoint).GetLength() +
				(area.getCenter() - conPoint).GetLength();
		}

		area_item(searchpath_base* b, const SquareMatrix<int>& a) :
			base(b),
			checklistRows(0),
			checklistRowStart(0),
			checklistRowHeight(5),
			checklistCols(0),
			checklistColStart(0),
			checklistColWidth(5),
			area(a),
			lastArea(NULL),
			dist_from_source(0) {}

		// iterates over all checklist points
		// calls given action with 2 parameters:
		//    VectorD2<int> p, VectorD2<int> dist
		// p is the point inside of the area, dist the change to the target (p+dist is therefore the new position)
		// if the returned value by the action is false, it will break
		template<typename _action>
		void forEachChecklistItem(_action action) {
			int i;
			VectorD2<int> p, dist;

			// TODO: this sorting is not needed anymore here
			typedef std::multiset< VectorD2<int>, VectorD2__absolute_less<int> > p_set;
			// the set point here defines, where the 'best' point is (the sorting depends on it)
			p_set points(VectorD2__absolute_less<int>( base->target )); // + getCenter()*2 - start) / 2));

			// insert now the points to the list
			// the list will sort itself

			// left
			p.x = area.v1.x;
			dist.x = -checklistRowHeight; dist.y = 0;
			for(i = 0; i < checklistRows; i++) {
				p.y = area.v1.y + checklistRowStart + i*checklistRowHeight;
				points.insert(p);
			}

			// right
			p.x = area.v2.x;
			dist.x = checklistRowHeight; dist.y = 0;
			for(i = 0; i < checklistRows; i++) {
				p.y = area.v1.y + checklistRowStart + i*checklistRowHeight;
				points.insert(p);
			}

			// top
			p.y = area.v1.y;
			dist.x = 0; dist.y = -checklistColWidth;
			for(i = 0; i < checklistCols; i++) {
				p.x = area.v1.x + checklistColStart + i*checklistColWidth;
				points.insert(p);
			}

			// bottom
			p.y = area.v2.y;
			dist.x = 0; dist.y = checklistColWidth;
			for(i = 0; i < checklistCols; i++) {
				p.x = area.v1.x + checklistColStart + i*checklistColWidth;
				points.insert(p);
			}

			// the list is sorted, the closest (to the target) comes first
			for(p_set::iterator it = points.begin(); it != points.end(); it++) {
				if(it->x == area.v1.x) { // left
					dist.x = -checklistRowHeight; dist.y = 0;
				} else if(it->x == area.v2.x) { // right
					dist.x = checklistRowHeight; dist.y = 0;
				} else if(it->y == area.v1.y) { // top
					dist.x = 0; dist.y = -checklistColWidth;
				} else { // bottom
					dist.x = 0; dist.y = checklistColWidth;
				}
				if(!action(*it, dist)) return;
			}

		}

		class check_checkpoint {
		public:
			searchpath_base* base;
			area_item* myArea;
			float bestNodeLen; // this can be done better...

			check_checkpoint(searchpath_base* b, area_item* a) :
				base(b),
				myArea(a),
				bestNodeLen(-1) {}

			// this will be called by forEachChecklistItem
			// pt is the checkpoint and dist the change to the new target (it means we want from pt to pt+dist)
			// it will search for the best node starting at the specific pos
			bool operator() (VectorD2<int> pt, VectorD2<int> dist) {
				bool trace = true;
				VectorD2<int> dir, start;
				unsigned short left, right;
				if(abs(dist.x) >= abs(dist.y))
					dir.y = 1;
				else
					dir.x = 1;
				start = pt;
				pt += dir;

				// ensure, that the way has at least the width of wormsize
				game.gameMap()->lockFlags(false);
				trace = simpleTraceLine(pt, dist, PX_ROCK);
				for(left = 0; trace && left < wormsize; pt += dir) {
					trace = simpleTraceLine(pt, dist, PX_ROCK);
					if(trace) left++;
				}
				pt = start - dir; trace = true;
				for(right = 0; trace && right < (wormsize-left); right++, pt -= dir) {
					trace = simpleTraceLine(pt, dist, PX_ROCK);
					if(trace) right++;
				}
				game.gameMap()->unlockFlags(false);

				// is there enough space?
				if(left+right >= wormsize) {
					base->addAreaNode( pt + dist, myArea );
				}

				// continue the search
				return true;
			}
		}; // class check_checkpoint

		void process() {
			// add all successor-nodes to areas_stack
			forEachChecklistItem( check_checkpoint(base, this) );
		}

	}; // class area_item

	// area_set keeps track about all available areas. because there can be areas with the same area_v1, this must be a multiset.
	typedef std::multiset< area_item*, area_item::area_v1__less > area_set;
	// area_stack_set is the dynamic stack in the pathfinding algo
	typedef std::set< area_item*, area_item::expected_min_total_dist__less > area_stack_set;
	typedef std::set< NEW_ai_node_t* > node_set;

	// these neccessary attributes have to be set manually
	area_set areas; // set of all created areas
	node_set nodes; // set of all created nodes
	VectorD2<int> start, target;

	area_stack_set areas_stack; // set of areas used by the searching algorithm

	searchpath_base() :
		resulted_path(NULL),
		thread(NULL),
		thread_is_ready(true),
		break_thread_signal(0),
		restart_thread_searching_signal(0) {
		thread = threadPool->start(threadSearch, this, "AI worm pathfinding");
		if(!thread)
			errors << "could not create AI thread" << endl;
	}

	~searchpath_base() {
		// thread cleaning up
		breakThreadSignal();
		if(thread) threadPool->wait(thread, NULL);
		else warnings << "AI thread already uninitialized" << endl;
		thread = NULL;
		
		clear();
	}

	void removePathFromList(NEW_ai_node_t* start) {
		for(NEW_ai_node_t* node = start; node; node = node->psNext)
			nodes.erase(node);
	}

private:
	void clear() {
		clear_areas();
		clear_nodes();
	}

	void clear_areas() {
		for(area_set::iterator it = areas.begin(); it != areas.end(); it++) {
			delete *it;
		}
		areas.clear();
	}

	void clear_nodes() {
		for(node_set::iterator it = nodes.begin(); it != nodes.end(); it++) {
			delete *it;
		}
		nodes.clear();
	}

	// searches for an overleading area and returns the first
	// returns NULL, if none found
	area_item* getArea(VectorD2<int> p) {
		// TODO: area_v1__less is not used here
		// (take a look at pseudoless)
		for(area_set::iterator it = areas.begin(); it != areas.end() && (*it)->area.v1 <= p; it++) {
			if((*it)->area.isInDefinedArea(p))
				return *it;
		}

#ifdef _AI_DEBUG
/*		printf("getArea( %i, %i )\n", p.x, p.y);
		printf("  don't find an underlying area\n");
		printf("  areas = {\n");
		for(area_set::iterator it = areas.begin(); it != areas.end(); it++) {
			printf("		( %i, %i, %i, %i )%s,\n",
				(*it)->area.v1.x, (*it)->area.v1.y,
				(*it)->area.v2.x, (*it)->area.v2.y,
				((*it)->area.v1 <= p) ? "" : " (*)");
		}
		printf("     }\n"); */
#endif

		return NULL;
	}

	area_item* getBestArea() {
		// areas_stack is sorted and we have the best solution at the beginning
		area_stack_set::iterator it = areas_stack.begin();
		if(it == areas_stack.end()) return NULL;
		return *it;
	}

	NEW_ai_node_t* buildPath(area_item* lastArea) {
		NEW_ai_node_t* last_node = createNewAiNode(target);
		nodes.insert(last_node);
		area_item* a = lastArea;

		while(a != NULL) {
			NEW_ai_node_t* node = createNewAiNode(a->area.getCenter());
			node->psNext = last_node;
			last_node->psPrev = node;
			nodes.insert(node);
			last_node = node;

			node = createNewAiNode(a->getConnectorPointForArea(a->lastArea));
			node->psNext = last_node;
			last_node->psPrev = node;
			nodes.insert(node);
			last_node = node;

			a = a->lastArea;
		}

		return last_node;
	}

	// it searches for the path (dynamic algo, sort of A*)
	NEW_ai_node_t* findPath(VectorD2<int> start) {
		areas_stack.clear();
		addAreaNode(start, NULL);

		while(areas_stack.size() > 0) {
			SDL_Delay(1); // lower priority to this thread
			if(shouldBreakThread() || shouldRestartThread() || !game.gameMap()->getCreated()) return NULL;

			area_item* a = getBestArea();
			areas_stack.erase(a);

			// can we just finish with the search?
			game.gameMap()->lockFlags(false);
			if(traceWormLine(target, a->area.getCenter())) {
				game.gameMap()->unlockFlags(false);
				// yippieh!
				return buildPath(a);
			}
			game.gameMap()->unlockFlags(false);

			a->process();
		}

		return NULL;
	}

public:

	// add new area as a node to path
	// start is location (so an area surrounding start is searched)
	void addAreaNode(VectorD2<int> start, area_item* lastArea) {

		// is the start inside of the map?
		if(start.x < 0 || (uint)start.x >= game.gameMap()->GetWidth()
		|| start.y < 0 || (uint)start.y >= game.gameMap()->GetHeight())
			return;

		// look around for an existing area here
		area_item* a = getArea(start);
		if(a) { // we found an area which includes this point
			if(a->expected_min_total_dist() > a->expected_min_total_dist(lastArea)) {
				areas_stack.erase(a);
				a->setLastArea(lastArea);
				areas_stack.insert(a);
			}
			return;
		}

		// get the max area (rectangle) around us
		game.gameMap()->lockFlags(false);
		SquareMatrix<int> area = getMaxFreeArea(start, PX_ROCK);
		game.gameMap()->unlockFlags(false);
		// add only if area is big enough
		if(area.v2.x-area.v1.x >= wormsize && area.v2.y-area.v1.y >= wormsize) {
			a = new area_item(this, area);
			a->initChecklists();
			a->setLastArea(lastArea);
			areas.insert(a);
			areas_stack.insert(a);
			// and search
		}
	}

	// this function will start the search, if it was not started right now
	// WARNING: the searcher-thread will clear all current saved nodes
	bool startThreadSearch() {
		if(game.state < Game::S_Preparing) {
			errors << "AI searchpath: cannot search yet, game not ready" << endl;
			return false;
		}
		
		// if we are still searching, do nothing
		if(!isReady()) return false;

		// this is the signal to start the search
		setReady(false);
		return true;
	}

private:
	// main-function used by the thread
	static Result threadSearch(void* b) {

		searchpath_base* base = (searchpath_base*)b;
		NEW_ai_node_t* ret;

		while(true) {
			// sleep a little bit while we have nothing to do...
			while(base->isReady()) {
				// was there a break-signal?
				if(base->shouldBreakThread()) {
					//printf("got break signal(1) for %i\n", (long)base);
					return true;
				}
				SDL_Delay(100);
			}

			base->resulted_path = NULL;
			base->clear(); // this is save and important here, else we would have invalid pointers

			// start the main search
			ret = base->findPath(base->start);

			// finishing the result
			base->completeNodesInfo(ret);
			base->simplifyPath(ret);
			base->splitUpNodes(ret, NULL);
			base->resulted_path = ret;

			if(base->shouldRestartThread()) {
				// HINT: both locks (of shouldRestartThread and the following) are seperated
				//       this don't make any trouble, because here is the only place where we
				//       reset it and we have always restart_thread_searching_signal==true here
				Mutex::ScopedLock lock(base->mutex);
				base->restart_thread_searching_signal = 0;
				base->start = base->restart_thread_searching_newdata.start;
				base->target = base->restart_thread_searching_newdata.target;
				continue;
			}

			// we are ready now
			base->setReady(true);
		}

		return "should not happen";
	}

	
	// HINT: threadSearch is the only function, who should set this to true again!
	// a set to false means for threadSearch, that it should start the search now
	void setReady(bool state) {
		Mutex::ScopedLock lock(mutex);
		thread_is_ready = state;
	}
	
public:
	bool isReady() {
		Mutex::ScopedLock lock(mutex);
		return thread_is_ready;
	}

	// WARNING: not thread safe; call isReady before
	NEW_ai_node_t* resultedPath() {
		return resulted_path;
	}

	void restartThreadSearch(VectorD2<int> newstart, VectorD2<int> newtarget) {
		// set signal
		Mutex::ScopedLock lock(mutex);
		thread_is_ready = false;
		restart_thread_searching_newdata.start = newstart;
		restart_thread_searching_newdata.target = newtarget;
		// HINT: the reading of this isn't synchronized
		restart_thread_searching_signal = 1;
	}

private:
	NEW_ai_node_t* resulted_path;
	ThreadPoolItem* thread;
	Mutex mutex;
	bool thread_is_ready;
	int break_thread_signal;
	int restart_thread_searching_signal;
	class start_target_pair { public:
		VectorD2<int> start, target;
	} restart_thread_searching_newdata;

	void breakThreadSignal() {
		// we don't need more thread-safety here, because this will not fail
		break_thread_signal = 1;
	}

	bool shouldBreakThread() {
		return (break_thread_signal != 0);
	}

public:
	bool shouldRestartThread() {
		return (restart_thread_searching_signal != 0);
	}

private:
	void completeNodesInfo(NEW_ai_node_t* start) {
		NEW_ai_node_t* last = NULL;
		for(NEW_ai_node_t* n = start; n; n = n->psNext) {
			n->psPrev = last;
			last = n;
		}
	}

	void splitUpNodes(NEW_ai_node_t* start, NEW_ai_node_t* end) {
		NEW_ai_node_t* tmpnode = NULL;
		short s1, s2;
		static const unsigned short dist = 50;
		for(NEW_ai_node_t* n = start; n && n->psNext && n != end; n = n->psNext) {
			s1 = (n->fX > n->psNext->fX) ? 1 : -1;
			s2 = (n->fY > n->psNext->fY) ? 1 : -1;
			if(s1*(n->fX - n->psNext->fX) > dist || s2*(n->fY - n->psNext->fY) > dist) {
				tmpnode = new NEW_ai_node_t;
				if(tmpnode) {
					nodes.insert(tmpnode);
					if(s1*(n->fX - n->psNext->fX) >= s2*(n->fY - n->psNext->fY)) {
						tmpnode->fX = n->fX - s1*dist;
						tmpnode->fY = n->fY
							- s1*dist*(n->fY - n->psNext->fY)/(n->fX - n->psNext->fX);
					} else {
						tmpnode->fY = n->fY - s2*dist;
						tmpnode->fX = n->fX
							- s2*dist*(n->fX - n->psNext->fX)/(n->fY - n->psNext->fY);
					}
					tmpnode->psNext = n->psNext;
					tmpnode->psPrev = n;
					n->psNext->psPrev = tmpnode;
					n->psNext = tmpnode;
				}
			}
		}
	}


	void simplifyPath(NEW_ai_node_t* start) {
		NEW_ai_node_t* node = NULL;
		NEW_ai_node_t* last_node = NULL;
		NEW_ai_node_t* closest_node = NULL;
		unsigned short count = 0;
		float dist, len;

		// short up
		for(node = start; node; node = node->psNext)  {
			len = 0;
			last_node = node;
			for(closest_node = node, count = 0; closest_node; closest_node = closest_node->psNext, count++) {
				len += CVec(closest_node->fX - last_node->fX, closest_node->fY - last_node->fY).GetLength2();
				dist = CVec(closest_node->fX - node->fX, closest_node->fY - node->fY).GetLength2();
				if(count >= 3
				&& dist < len
				&& traceWormLine(CVec(closest_node->fX,closest_node->fY),CVec(node->fX,node->fY))) {
					node->psNext = closest_node;
					closest_node->psPrev = node;
					len = dist;
				}
				last_node = closest_node;
			}
		}

		// simplify
		for(node = start; node; node = node->psNext) {
			// While we see the two nodes, delete all nodes between them and skip to next node
			count = 0;
			while (closest_node && traceWormLine(CVec(closest_node->fX,closest_node->fY),CVec(node->fX,node->fY)))  {
				if(count >= 2) {
					node->psNext = closest_node;
					closest_node->psPrev = node;
				}
				closest_node = closest_node->psNext;
				count++;
			}
		}
	}

}; // class searchpath_base



///////////////////
// Initialize the AI
bool CWormBotInputHandler::AI_Initialize() {
    // Because this can be called multiple times, shutdown any previous allocated data
    AI_Shutdown();

	if(game.gameMap() == NULL) {
		errors << "CWormBotInputHandler::AI_Initialize(): map not set" << endl;
		return false;
	}

    m_worm->fLastCarve = AbsTime();
    cStuckPos = CVec(-999,-999);
    fStuckTime = 0;
    fLastPathUpdate = AbsTime();
	fLastJump = AbsTime();
	fLastCreated = AbsTime();
	fLastThink = AbsTime();
    bStuck = false;
	bPathFinished = true;
	iAiGameType = GAM_OTHER;
	nAITargetType = AIT_NONE;
	nAIState = AI_THINK;
	fLastFace = AbsTime();
	fLastShoot = AbsTime();
	fLastCompleting = AbsTime();
	fLastGoBack = AbsTime();


	fCanShootTime = 0;

	fRopeAttachedTime = 0;
	fRopeHookFallingTime = 0;

	if(pathSearcher)
		warnings << "pathSearcher is already initialized" << endl;
	else
		pathSearcher = new searchpath_base;
	if(!pathSearcher) {
		errors << "cannot initialize pathSearcher" << endl;
		return false;
	}

    return true;
}


///////////////////
// Shutdown the AI stuff
void CWormBotInputHandler::AI_Shutdown()
{
	if(pathSearcher)
		delete pathSearcher;
	pathSearcher = NULL;

	// in every case, the nodes of the current path are not handled by pathSearcher
	delete_ai_nodes(NEW_psPath);

	NEW_psPath = NULL;
	NEW_psCurrentNode = NULL;
	NEW_psLastNode = NULL;
}

void CWormBotInputHandler::deleteThis() {
	AI_Shutdown();
	CWormInputHandler::deleteThis();
}



/*
  Algorithm:
  ----------

  1) Find nearest worm and set it as a target
  2) If we are within a good distance, aim and shoot the target
  3) If we are too far, try and get closer by digging and ninja rope

*/

#ifdef _AI_DEBUG
class debug_print_col {
public:
	SmartPointer<SDL_Surface> bmpDest;
	debug_print_col(const SmartPointer<SDL_Surface> & dest=NULL) : bmpDest(dest) {}

	bool operator()(int x, int y) const {
		if(!bmpDest.get())
			return false;
		if(x*2-4 >= 0 && x*2+4 < bmpDest.get()->w
		&& y*2-4 >= 0 && y*2+4 < bmpDest.get()->h)
			DrawRectFill(bmpDest.get(),x*2-4,y*2-4,x*2+4,y*2+4,Color(255,255,0));
		else
			return false;
		return true;
	}
};

void do_some_tests_with_fastTraceLine() {
	CVec start, target;
	start.x = (float)(rand() % game.gameMap()->GetWidth());
	start.y = (float)(rand() % game.gameMap()->GetHeight());
	target.x = (float)(rand() % game.gameMap()->GetWidth());
	target.y = (float)(rand() % game.gameMap()->GetHeight());

	debug_print_col printer(game.gameMap()->GetDebugImage());
	fastTraceLine(target, start, PX_ROCK, printer);
}
#endif


void CWormBotInputHandler::AI_Respawn() {
	if(findNewTarget()) {
		AI_CreatePath(true);
	}
}


// called when the game starts (after weapon selection)
void CWormBotInputHandler::startGame() {
	if(	cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_DEATHMATCH ||
		cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_TEAMDEATH ||
		cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_TAG ||
		cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_HIDEANDSEEK ||
		cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_CTF ||
	    cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_RACE ||
	    cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_TEAMRACE)
	{
		// it's fine, we support that game mode
	}
	else if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode == NULL) {
		warnings << "bot: gamemode " << (std::string)cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->name << " is unknown" << endl;
	} else {
		warnings << "bot: support for gamemode " << cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode->Name()
				<< " is currently not implemented" << endl;
	}
}

///////////////////
// Simulate the AI
void CWormBotInputHandler::getInput() {
	if(!m_worm->getAlive()) {
		if(m_worm->bCanRespawnNow)
			m_worm->bRespawnRequested = true;
		return;
	}
	
	worm_state_t *ws = &m_worm->tState.write();
	

	// Init the ws
	ws->reset();
	
	// Behave like humans and don't play immediatelly after spawn
	if ((tLX->currentTime - m_worm->fSpawnTime) < 0.4f)
		return;
	
	// Update bOnGround, so we don't have to use CheckOnGround every time we need it
	m_worm->bOnGround = m_worm->CheckOnGround();


    tLX->debug_string = "";

	iRandomSpread = 0;
	fLastRandomChange = AbsTime();

    // Every 3 seconds we run the think function
	float thinkInterval = 3.0f;
	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_CTF)
		thinkInterval = 0.5f; // recheck more often
    if(tLX->currentTime - fLastThink > thinkInterval && nAIState != AI_THINK)
        nAIState = AI_THINK;

	// check more often if the path isn't finished yet
	if(tLX->currentTime - fLastThink > 0.5 && !bPathFinished)
		nAIState = AI_THINK;

    // Make sure the worm is good
    if(nAITargetType == AIT_WORM) {
		CWorm* w = game.wormById(nAITargetWormId, false);
		if(!w || !w->getAlive() || w->getAFK() != AFK_BACK_ONLINE) {
			nAIState = AI_THINK;
		}
	}
	
	// Carve always; makes no problems and can only help
	AI_Carve();

    // If we have a good shooting 'solution', shoot
    if(AI_Shoot()) {

		// jump, move and carve around
		/*AI_Jump();
		ws->bMove = true;

   		if(bOnGround && fRopeAttachedTime >= 0.3f && !AI_IsInAir(vPos))
   			cNinjaRope.Release();*/

		// Don't move in the direction of projectiles when shooting
		if (m_worm->cNinjaRope.get().isAttached())  {
			CVec force = m_worm->cNinjaRope.get().GetForce();
			float rope_angle = (float)atan(force.x / force.y);
			if (force.x < 0 || force.y > 0)
				rope_angle = -rope_angle;
			rope_angle = RAD2DEG(rope_angle);

			if (fabs(m_worm->fAngle - rope_angle) <= 50)
				m_worm->cNinjaRope.write().Clear();
		}
		m_worm->tState.write().bMove = false;

		
    } else {

		// Reload weapons when we can't shoot
		AI_ReloadWeapons();

		// Process depending on our current state
		switch(nAIState) {

			// Think; We spawn in this state
			case AI_THINK:
				AI_Think();
				break;

			// Moving towards a target
			case AI_MOVINGTOTARGET:
				AI_MoveToTarget();
				break;
				
			default:
				nAIState = AI_THINK;
				AI_Think();
				break;
		}

    }

    // we have no strafing for bots at the moment
	m_worm->iMoveDirectionSide = m_worm->iFaceDirectionSide;
}

static bool moveToOwnBase(int t, CVec& pos) {
	Flag* ownFlag = cClient->flagInfo()->getFlag(t);
	if(!ownFlag) return false; // strange
	pos = ownFlag->spawnPoint.pos;
	return true;
}

static bool findEnemyBase(CWorm* w, CVec& pos) {	
	float lastDist = 999999999.0f;
	bool success = false;
	for(int t = 0; t < MAX_TEAMS; ++t) {
		if(t == w->getTeam()) continue;
		Flag* flag = cClient->flagInfo()->getFlag(t);
		if(!flag) continue;
		
		float dist = (w->getPos() - flag->spawnPoint.pos).GetLength();
		if(dist < lastDist) {
			lastDist = dist;
			pos = flag->spawnPoint.pos;
			success = true;
		}
	}

	return success;
}

static bool findEnemyFlag(CWorm* w, CVec& pos) {	
	float lastDist = 999999999.0f;
	bool success = false;
	for(int t = 0; t < MAX_TEAMS; ++t) {
		if(t == w->getTeam()) continue;
		Flag* flag = cClient->flagInfo()->getFlag(t);
		if(!flag) continue;
		if(flag->holderWorm >= 0) continue;
		
		float dist = (w->getPos() - flag->getPos()).GetLength();
		if(dist < lastDist) {
			lastDist = dist;
			pos = flag->getPos();
			success = true;
		}
	}
	if(success) return true;
	
	return findEnemyBase(w, pos);
}

static Flag* teamHasEnemyFlag(int t) {
	for_each_iterator(CWorm*, w, game.worms()) {
		if(w->get()->getTeam() != t) continue;
		Flag* flag = cClient->flagInfo()->getFlagOfWorm(w->get()->getID());
		if(flag) return flag;
	}
	return NULL;
}

bool CWormBotInputHandler::findNewTarget() {
	if(	(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_TAG && m_worm->getTagIT()) ) {
		CWorm* w = nearestEnemyWorm();
		if(!w) return findRandomSpot();
		
		float l = (w->getPos() - m_worm->vPos).GetLength2();
		if(l < 30*30) // search new spot iff <30 pixels away from me
			return findRandomSpot();
		else
			return true;
	}
	else if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_HIDEANDSEEK)
	{
		if(m_worm->getTeam() == (int)HIDEANDSEEK_HIDER) {
			CWorm* w = nearestEnemyWorm();
			if(!w) return findRandomSpot();
			
			float l = (w->getPos() - m_worm->vPos).GetLength2();
			if(l < 30*30) // search new spot iff <30 pixels away from me
				return findRandomSpot();
			else
				return true;
		}
		else { // we are seeker
			CWorm* w = nearestEnemyWorm();
			if(!w)
				// just move randomly
				return findRandomSpot();
			
			nAITargetWormId = w->getID();
			nAITargetType = AIT_WORM;
			nAIState = AI_MOVINGTOTARGET;
			return true;
		}
	}
	else if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_CTF) {
		Flag* wormFlag = cClient->flagInfo()->getFlagOfWorm(m_worm->getID());
		bool success = false;
		if(wormFlag != NULL) { // we have an enemy flag
			if(cClient->getTeamWormCount(m_worm->getTeam()) == 1) { // we are alone in the team
				Flag* ownFlag = cClient->flagInfo()->getFlag(m_worm->getTeam());
				if(ownFlag) {
					if(ownFlag->holderWorm >= 0) {
						nAITargetWormId = ownFlag->holderWorm;
						nAITargetType = AIT_WORM;
						nAIState = AI_MOVINGTOTARGET;					
					}
					else {
						cPosTarget = ownFlag->getPos();
						success = true;
					}
				}
			}
			else { // not alone in our team
				success = moveToOwnBase(m_worm->getTeam(), cPosTarget);
			}
		}
		else { // we don't hold any flag
			if(teamHasEnemyFlag(m_worm->getTeam())) {
				Flag* ownFlag = cClient->flagInfo()->getFlag(m_worm->getTeam());
				if(ownFlag) {
					if(ownFlag->holderWorm >= 0) {
						nAITargetWormId = ownFlag->holderWorm;
						nAITargetType = AIT_WORM;
						nAIState = AI_MOVINGTOTARGET;					
					}
					else {
						cPosTarget = ownFlag->getPos();
						success = true;
					}
				}
			} else
				success = findEnemyFlag(m_worm, cPosTarget);
		}
		if(success) {
			nAITargetType = AIT_POSITION;
			nAIState = AI_MOVINGTOTARGET;
			return true;
		}
	}
	else if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_RACE || cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_TEAMRACE) {		
		int t = m_worm->getID();
		if(cClient->isTeamGame()) t = m_worm->getTeam();

		Flag* flag = cClient->flagInfo()->getFlag(t);
		if(!flag) return false; // strange
		
		nAITargetType = AIT_POSITION;
		nAIState = AI_MOVINGTOTARGET;
		cPosTarget = flag->spawnPoint.pos;
		return true;
	}
	else {
		// TODO: also check for GM_DEMOLITIONS (whereby killing other worms is not completly bad in this mode)
		
		// find new target and reset the path
	
    	// Search for an unfriendly worm
		CWorm* targetWorm = findTarget();

    	// Any unfriendlies?
		if(targetWorm) {
        	// We have an unfriendly target, so change to a 'move-to-target' state
			nAITargetWormId = targetWorm->getID();
			nAITargetType = AIT_WORM;
			nAIState = AI_MOVINGTOTARGET;
			return true;
		}
		
		return false;
	}
	
	return false;
}

///////////////////
// Find a target worm
CWorm *CWormBotInputHandler::findTarget()
{
	CWorm	*trg = NULL;
	CWorm	*nonsight_trg = NULL;
	float	fDistance = -1;
	float	fSightDistance = -1;
	
    //
	// Just find the closest worm
	//

	for_each_iterator(CWorm*, w_, game.aliveWorms()) {
		CWorm* w = w_->get();

		if(!w->isVisible(m_worm)) continue;

		// Make sure i don't target myself
		if(w->getID() == m_worm->iID)
			continue;

		// don't target AFK worms
		if(w->getAFK() != AFK_BACK_ONLINE)
			continue;
		
		// If this is a team game, don't target my team mates
		if(cClient->isTeamGame() && w->getTeam() == m_worm->iTeam)
			continue;

		// If this is a game of tag, only target the worm it (unless it's me)
		if(cClient->isTagGame() && !w->getTagIT() && !m_worm->bTagIT)
			continue;


		// Calculate distance between us two
		float l = (w->getPos() - m_worm->vPos).GetLength2();

		// Prefer targets we have free line of sight to
		float length;
		int type;
		m_worm->traceLine(w->getPos(),&length,&type,1);
		if (! (type & PX_ROCK))  {
			// Line of sight not blocked
			if (fSightDistance < 0 || l < fSightDistance)  {
				trg = w;
				fSightDistance = l;
				if (fDistance < 0 || l < fDistance)  {
					nonsight_trg = w;
					fDistance = l;
				}
			}
		}
		else {
			// Line of sight blocked
			if(fDistance < 0 || l < fDistance) {
				nonsight_trg = w;
				fDistance = l;
			}
		}
	}

	// If the target we have line of sight to is too far, switch back to the closest target
	if (fSightDistance-fDistance > 50.0f || /*TODO: in rifles we can shoot target far away!*/iAiGameType == GAM_RIFLES || trg == NULL)  {
		if (nonsight_trg)
			trg = nonsight_trg;
	}

    return trg;
}


CWorm* CWormBotInputHandler::nearestEnemyWorm() {
	CWorm	*trg = NULL;
	float	fDistance = -1;
	float	fSightDistance = -1;

	//
	// Just find the closest worm
	//
	
	for_each_iterator(CWorm*, w_, game.aliveWorms()) {
		CWorm* w = w_->get();

		if(!w->isVisible(m_worm)) continue;
		
		// Make sure i don't target myself
		if(w->getID() == m_worm->iID)
			continue;
		
		// don't target AFK worms
		if(w->getAFK() != AFK_BACK_ONLINE)
			continue;
		
		// If this is a team game, don't target my team mates
		if(cClient->isTeamGame() && w->getTeam() == m_worm->iTeam)
			continue;

		// If this is a game of tag, only target the worm it (unless it's me)
		if(cClient->isTagGame() && !w->getTagIT() && !m_worm->bTagIT)
			continue;

		// Calculate distance between us two
		float l = (w->getPos() - m_worm->vPos).GetLength2();

		// Prefer targets we have free line of sight to
		float length;
		int type;
		m_worm->traceLine(w->getPos(),&length,&type,1);
		if (! (type & PX_ROCK))  {
			// Line of sight not blocked
			if (fSightDistance < 0 || l < fSightDistance)  {
				trg = w;
				fSightDistance = l;
				if (fDistance < 0 || l < fDistance)  {
					fDistance = l;
				}
			}
		}
		else {
			// Line of sight blocked
			if(fDistance < 0 || l < fDistance) {
				fDistance = l;
			}
		}
	}

	return trg;

}



///////////////////
// Think State
void CWormBotInputHandler::AI_Think()
{
    /*
      We start of in an think state. When we're here we decide what we should do.
      If there is an unfriendly worm, or a game target we deal with it.
      In the event that we have no unfriendly worms or game targets we should remain in the idle state.
      While idling we can walk around to appear non-static, reload weapons or grab a bonus.
    */

    // Clear the state
    nAITargetWormId = -1;
    psBonusTarget = NULL;
    nAITargetType = AIT_NONE;
    fLastThink = tLX->currentTime;


    // Reload our weapons in idle mode
    AI_ReloadWeapons();


	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() != GM_RACE && cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() != GM_TEAMRACE) {
    // If our health is less than 15% (critical), our main priority is health
    if(m_worm->health < 15)
        if(AI_FindHealth())
            return;

	{
		// search for weapon if we need some
		int wpnNum = 0;
		for(size_t i = 0; i < m_worm->tWeapons.size(); ++i)
			if(m_worm->tWeapons[i].weapon())
				wpnNum++;
		if(wpnNum < (int)m_worm->tWeapons.size()) {
			if(AI_FindBonus(BNS_WEAPON)) {
				// select disabled weapon (which should be replaced by bonus)
				for(size_t i = 0; i < m_worm->tWeapons.size(); ++i)
					if(!m_worm->tWeapons[i].weapon()) {
						m_worm->iCurrentWeapon = (int32_t)i;
						break;
					}
				return;
			}
		}
	}
	}
	
    // Search for an unfriendly worm
    if(findNewTarget()) {
		AI_CreatePath();
		return;
	}
	else
		fLastShoot = AbsTime(); // force new shoot

    // If we're down on health (less than 80%) we should look for a health bonus
    if(m_worm->health < 80) {
        //printf("we should look for health\n");
        if(AI_FindHealth())
            return;
    }


    /*
      If we get here that means we have nothing to do
    */


    //
    // Typically, high ground is safer. So if we don't have anything to do, lets up up
    //

    // Our target already on high ground?
    if(cPosTarget.y < 75 && nAIState == AI_MOVINGTOTARGET)  {

		// Nothing todo, so go find some health if we even slightly need it
		if(m_worm->health < 100) {
			if(AI_FindHealth())
				return;
		}
        return;
	}

	if(findRandomSpot(true))
		AI_CreatePath();
 }

bool CWormBotInputHandler::findRandomSpot(bool highSpot) {
	float w = (float)game.gameMap()->GetWidth();
	float h = (float)game.gameMap()->GetHeight();
	if(highSpot)
		h /= 5.0f; // little hack to go higher
	
	CMap::PixelFlagAccess flags(game.gameMap());
	
    // Find a random spot to go to high in the level
    //printf("I don't find any target, so let's get somewhere (high)\n");
	for(int c=0; c<10; c++) {
		long x = (long)(fabs(GetRandomNum()) * w);
		long y = (long)(fabs(GetRandomNum()) * h);

		if(!game.gameMap()->IsGoodSpawnPoint(flags, x, y))
			continue;

		// Set the target
		cPosTarget = CVec((float)x, (float)y);
		nAITargetType = AIT_POSITION;
		nAIState = AI_MOVINGTOTARGET;
		return true;
	}

	return false;
}


bool CWormBotInputHandler::AI_FindHealth() {
	return AI_FindBonus(BNS_HEALTH);
}

///////////////////
// Find a health pack
// Returns true if we found one
bool CWormBotInputHandler::AI_FindBonus(int bonustype)
{
	if (!cClient->getGameLobby()[FT_Bonuses])
		return false;

    CBonus  *pcBonusList = cClient->getBonusList();
    short     i;
    CBonus  *pcBonus = NULL;
    float   dist2 = -1;
	float d2;

    // Find the closest health bonus
    for(i=0; i<MAX_BONUSES; i++) {
        if(pcBonusList[i].getUsed() && pcBonusList[i].getType() == bonustype) {

            d2 = (pcBonusList[i].getPosition() - m_worm->vPos).GetLength2();

            if(dist2 < 0 || d2 < dist2) {
                pcBonus = &pcBonusList[i];
                dist2 = d2;
            }
        }
    }


    // TODO: Verify that the target is not in some small cavern that is hard to get to


    // If we have found a bonus, setup the state to move towards it
    if(dist2 >= 0) {
        psBonusTarget = pcBonus;
        nAITargetType = AIT_BONUS;
        nAIState = AI_MOVINGTOTARGET;
		AI_CreatePath();
		return true;
    }

    return false;
}


///////////////////
// Reloads the weapons
void CWormBotInputHandler::AI_ReloadWeapons()
{
    // Go through reloading the weapons
	for(ushort i=0; i<m_worm->tWeapons.size(); i++) {
        if(m_worm->tWeapons[i].Reloading) {
            m_worm->iCurrentWeapon = i;
            break;
        }
    }
}


///////////////////
// Get the target's position
// Also checks the target and resets to a think state if needed
CVec CWormBotInputHandler::AI_GetTargetPos()
{
    // Put the target into a position
    switch(nAITargetType) {

        // Bonus target
        case AIT_BONUS:
            if(psBonusTarget) {
                if(!psBonusTarget->getUsed())
                    nAIState = AI_THINK;
                return psBonusTarget->getPosition();
            }
            break;

        // Position target
        case AIT_POSITION:
            return cPosTarget;

        // Worm target
        case AIT_WORM:
        default:
        	if(nAITargetType != AIT_WORM)
        		nAIState = AI_THINK;
			CWorm* w = game.wormById(nAITargetWormId, false);
			if(!w || !w->getAlive() )
				nAIState = AI_THINK;
			else
				return w->getPos();
		break;

    }

    // No target
    nAIState = AI_THINK;
    return CVec(0,0);
}

static const float BotAngleSpeed = 100;

///////////////////
// Aim at a spot
// Returns true if we're aiming at it
bool CWormBotInputHandler::AI_SetAim(CVec cPos)
{
	TimeDiff   dt = tLX->fDeltaTime;
    CVec	tgPos = cPos;
	CVec	tgDir = tgPos - m_worm->vPos;
    bool    goodAim = false;

	float angleSpeed = BotAngleSpeed * dt.seconds();
	
	NormalizeVector(&tgDir);

	DIR_TYPE wantedDir = (tgDir.x < 0) ? DIR_LEFT : DIR_RIGHT;
	if (tLX->currentTime - fLastFace > 0.3f)  {  // prevent turning
		// Make me face the target
		m_worm->iFaceDirectionSide = wantedDir;

		fLastFace = tLX->currentTime;
	}
	
	// Aim at the target
	float ang = (float)atan2(tgDir.x, tgDir.y);
	ang = RAD2DEG(ang);

	if(wantedDir == DIR_LEFT)
		ang+=90;
	else
		ang = -ang + 90;

	// Clamp the angle
	ang = MAX((float)-90, ang);

	if(iAiDiffLevel < AI_XTREME) {
		static float angleDiff[AI_HARD + 1] = {0,0,0};
		static AbsTime lastAngleDiffUpdateTime;
		if(tLX->currentTime - lastAngleDiffUpdateTime > TimeDiff(0.5f)) {
			for(int aiLevel = AI_EASY; aiLevel < AI_XTREME; aiLevel++) {
				float maxdiff = float(AI_XTREME - aiLevel) / float(AI_XTREME);
				angleDiff[aiLevel] = GetRandomNum() * maxdiff * 50.0f;
			}
			lastAngleDiffUpdateTime = tLX->currentTime;
		}
		ang += angleDiff[iAiDiffLevel];
		ang = MAX(ang, -90.0f);		
	}
	
	// Move the angle at the same speed humans are allowed to move the angle
	if(ang > m_worm->fAngle)
		m_worm->fAngle += angleSpeed;
	else if(ang < m_worm->fAngle)
		m_worm->fAngle -= angleSpeed;

	// If the angle is within +/- 3 degrees, just snap it
    if( fabs(m_worm->fAngle - ang) < 3 )
		m_worm->fAngle = ang;

	if(fabs(m_worm->fAngle - ang) < 3 + 20 * float(AI_XTREME - iAiDiffLevel) / float(AI_XTREME))
		goodAim = true;
	
	// Clamp the angle
	m_worm->fAngle = CLAMP(m_worm->fAngle.get(), -90.0f, cClient->getGameLobby()[FT_FullAimAngle] ? 90.0f : 60.0f);

    return goodAim;
}


///////////////////
// A simpler method to get to a target
// Used if we have no path
void CWormBotInputHandler::AI_SimpleMove(bool bHaveTarget)
{
	worm_state_t *ws = &m_worm->tState.write();

    // Simple
	ws->bMove = true;
	ws->bShoot = false;
	ws->bJump = false;

    //strcpy(tLX->debug_string, "AI_SimpleMove invoked");

    cPosTarget = AI_GetTargetPos();
	if (nAITargetType == AIT_WORM && nAITargetWormId >= 0)
		cPosTarget = AI_FindShootingSpot();

    // Aim at the node
    bool aim = AI_SetAim(cPosTarget);

    // If our line is blocked, try some evasive measures
    float fDist = 0;
    int type = 0;
    m_worm->traceLine(cPosTarget, &fDist, &type, 1);
	if(fDist < 0.75f || cPosTarget.y < m_worm->vPos.get().y) {

        // Change direction
		if (bHaveTarget && (tLX->currentTime-fLastFace) > 1.0)  {
			m_worm->iFaceDirectionSide = OppositeDir((DIR_TYPE)(int)m_worm->iFaceDirectionSide);
			fLastFace = tLX->currentTime;
		}

        // Look up for a ninja throw
        aim = AI_SetAim(m_worm->vPos + CVec(GetRandomNum()*10, GetRandomNum()*10 + 10));
        if(aim) {
            const CVec dir = m_worm->getFaceDirection();
			m_worm->cNinjaRope.write().Shoot(dir);
        }

		// Jump and move
		else  {
			AI_Jump();
			ws->bMove = true;
			m_worm->cNinjaRope.write().Clear();
		}

        return;
    }

    // Release the ninja rope
	m_worm->cNinjaRope.write().Clear();
}

float fLastDirChange = 99999;

///////////////////
// Finds a suitable 'clearing' weapon
// A weapon used for making a path
// Returns -1 on failure
int CWormBotInputHandler::AI_FindClearingWeapon()
{
	const bool canIgnoreSelfHit = !(bool)cClient->getGameLobby()[FT_SelfInjure];
	
	if(!canIgnoreSelfHit && iAiGameType == GAM_MORTARS)
		return -1;

	// search a good projectile weapon
	for (size_t i=0; i<m_worm->tWeapons.size(); i++) {
		if(m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_PROJECTILE) {
			// TODO: not really all cases...
			Proj_ActionType type = m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type;

			// Nothing that could fall back onto us
			if (!canIgnoreSelfHit && m_worm->tWeapons[i].weapon()->Proj.Speed < 100.0f) {
				if (!m_worm->tWeapons[i].weapon()->Proj.Proj->UseCustomGravity || m_worm->tWeapons[i].weapon()->Proj.Proj->Gravity > 30)
					continue;
			}

			// Suspicious
			std::string name;
			name = m_worm->tWeapons[i].weapon()->Name;
			stringlwr(name);
			if(strincludes(name,"dirt") || strincludes(name,"napalm") || strincludes(name,"grenade") || strincludes(name,"nuke") || strincludes(name,"mine"))
				continue;

			// Nothing explosive or dirty
			if (type != PJ_DIRT && type != PJ_GREENDIRT && type != PJ_BOUNCE)
				if(!m_worm->tWeapons[i].Reloading)
					return (int)i;
		}
	}

	// accept also beam-weapons as a second choice
	for (size_t i=0; i<m_worm->tWeapons.size(); i++)
		if(m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_BEAM)
			return (int)i;

    // No suitable weapons
    return -1;
}



////////////////////
// Returns true, if the weapon can hit the target
// WARNING: works only when fAngle == AI_GetAimingAngle, which means the target has to be aimed
bool CWormBotInputHandler::weaponCanHit(float gravity, float speed, CVec cTrgPos)
{
	// Get the target position
	if(nAITargetWormId < 0)
		return false;

	// Get the projectile
	const wpnslot_t* wpnslot = m_worm->getWeapon(m_worm->getCurrentWeapon());
	const weapon_t* wpn = wpnslot ? wpnslot->weapon() : NULL;
	proj_t* wpnproj = wpn ? wpn->Proj.Proj : NULL;
	if(!wpnproj) // no valid weapon
		return false;

	if(iAiDiffLevel < AI_XTREME) {
		static float randomNums[4] = {0,0,0,0};
		static AbsTime lastRandomNumRefresh;
		if(tLX->currentTime - lastRandomNumRefresh > TimeDiff(0.5f)) {
			for(uint i = 0; i < sizeof(randomNums)/sizeof(randomNums[0]); ++i)
				randomNums[i] = GetRandomNum();
			lastRandomNumRefresh = tLX->currentTime;
		}
		
		float f = float(AI_XTREME - iAiDiffLevel) / float(AI_XTREME);
		gravity += randomNums[0] * f * 5;
		speed += randomNums[1] * f * 20.0f;
		cTrgPos.x += randomNums[2] * f * 50.0f;
		cTrgPos.y += randomNums[3] * f * 50.0f;		
	}
	
	// Exchange endpoints and velocity if needed
	float x_vel = 1;
	CVec from = m_worm->vPos;
	CVec to = cTrgPos;
	if (from.x > to.x)  {
		from = cTrgPos;
		to = m_worm->vPos;
		x_vel = -1;
	} else if (from.x == to.x)
		return true;

	// Get the parabolic trajectory
	// TODO: this calculation is wrong in some cases, someone who knows how to work with fAngle please fix it
	float alpha = DEG2RAD(m_worm->fAngle);
	if (cTrgPos.y > m_worm->vPos.get().y)
		alpha = -alpha;
	Parabola p(m_worm->vPos, alpha, cTrgPos);
	if (p.a > 0.1f)
		return false;

#ifdef DEBUG
	//game.gameMap()->ClearDebugImage();
#endif

	// Check
	float last_y = (p.a * (from.x + 5) * (from.x + 5) + p.b * (from.x + 5) + p.c);
	for (float x = from.x + 5; x < to.x; x++)  {
		float y = (p.a * x * x + p.b *x + p.c);

		// Rock or dirt, trajectory not free
		if (CProjectile::CheckCollision(wpnproj, 1, CVec(x, y), CVec(x_vel, y - last_y)))
			return false;

		last_y = y;

#ifdef DEBUG
		//PutPixel(game.gameMap()->GetDebugImage(), CLAMP((int)x, 0, (int)game.gameMap()->GetWidth()-1)*2, CLAMP((int)y, 0, (int)game.gameMap()->GetHeight()-1)*2, Color(0, 255, 0));
#endif
	}

	return true;
}


bool AI_GetAimingAngle(float v, float g, float x, float y, float *angle)
{
	// TODO: returns wron angles (too big) for mortars
	// Is it a fault of wrong parameters or wrong calculations?

	float v2 = v*v;
	float x2 = x*x;
	float g2 = g*g;
	float y2 = y*y;

	// Small hack - for small y-distance we want positive numbers
	if (fabs(y) < 3)  {
		if (y <= 0)
			y = -y;
	}

	float tmp1 = (float)fabs(-2*v2*y*g-g2*x2+v2*v2);

	// Validity check
	if (tmp1 < 0)  {
		*angle = 0;
		return false;
	}

	float tmp2 = -x2*(float)sqrt(tmp1)+x2*y*g+v2*(x2+2*y2);

	// Validity check
	if (tmp2 < 0)  {
		*angle = 0;
		return false;
	}

	float tmp3 = (float)sqrt(tmp1)-y*g+v2;

	// Validity check
	if (tmp3 <= 0 || x == 0)  {
		*angle = 60;
		return false;
	}

	// Get the angle
	*angle = (float)atan((float)sqrt(tmp2)/(x*(float)sqrt(tmp3)));
	if (x < 0)
		*angle = -(*angle);
	if (y > 0)
		*angle = -(*angle);

	// Convert to degrees
	*angle *= R2D;

	// Clamp the angle
	const float upperAngle = cClient->getGameLobby()[FT_FullAimAngle] ? 90.0f : 60.0f;
	if (*angle > upperAngle)  {
		*angle = upperAngle;
		return false;
	}
	if (*angle < -90)  {
		*angle = -90;
		return false;
	}

	// Ok!
	return true;

}


static bool canShootRightNowWithCurWeapon(CWorm* w) {
	if(game.gameScript()->gusEngineUsed())
		// TODO: reloading check and so on
		return true;
	
	// code from GameServer::WormShoot, CClient::PlayerShoot
	// and look also at CClient::ShootSpecial for special weapons like jetpack
	
	const wpnslot_t *Slot = w->getCurWeapon();
	if(!Slot) return false;
	
	if(Slot->Reloading)
		return false;
	
	if(Slot->LastFire>0)
		return false;
	
	if(!Slot->weapon())
		return false;

	if(Slot->weapon()->Type == WPN_SPECIAL) {
		switch(Slot->weapon()->Special) {
			case SPC_JETPACK: return true;
			default: return false;
		}
	}
	
	// Must be a projectile
	if(Slot->weapon()->Type != WPN_PROJECTILE && Slot->weapon()->Type != WPN_BEAM)
		return false;
	
	return true;
}


///////////////////
// Shoot!
// returns true if we want to do it or already doing it (also in the progress of aiming)
bool CWormBotInputHandler::AI_Shoot()
{
	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode && !cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode->Shoot(m_worm)) {
		// there is no shooting in this gamemode
		return false;
	}
	
	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_RACE || cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_TEAMRACE)
		// dont care about shooting in this mod, just try to be fast
		return false;
	
	if(!canShootRightNowWithCurWeapon(m_worm)) return false;
	
	// search for best target
 	CWorm* w = findTarget();
	if(!w) return false;
	
	CVec    cTrgPos = w->getPos();
    bool    bDirect = true;

	
    /*
      Here we check if we have a line of sight with the target.
      If we do, and our 'direct' firing weapons are loaded, we shoot.

      If the line of sight is blocked, we use 'indirect' shooting methods.
      We have to be careful with indirect, because if we're in a confined space, or the target is above us
      we shouldn't shoot at the target

      The aim of the game is killing worms, so we put a higher priority on shooting rather than
      thinking tactically to prevent injury to self

	  If we play a mortar game, make sure there's enough free space to shoot - avoid suicides
    */


    // If the target is too far away we can't shoot at all (but not when a rifle game)
	CVec diff = cTrgPos - m_worm->vPos;
    float d = diff.GetLength();
 /*   if(d > 300.0f && iAiGameType != GAM_RIFLES)
        return false; */

/*	// If we're on the target, simply shoot // this doesn't work in every case
	if(d < 10.0f)
		return true;
*/

	// TODO: first choose the weapon, and then use weaponCanHit instead of traceWeaponLine
	// hm, i would say, the other way around is better

	
    float fDist;
    int nType = -1;

	traceWeaponLine(cTrgPos, &fDist, &nType);

    // If target is blocked by rock we can't use direct firing
    if(nType & PX_ROCK)  {
		bDirect = false;
	}

	// Don't shoot teammates
	// TODO: why do we catch only teammates here and not the enemy worm?
	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->generalGameType == GMT_TEAMS && (nType & PX_WORM)) {
		//notes << "bot: we don't want shoot teammates" << endl;
		return false;
	}


	if(game.gameScript()->gusEngineUsed()) {
		// TODO: merge this with rest (we have to merge wpns for this)
		
		if(!bDirect) return false;
		if(!AI_SetAim(cTrgPos)) return false;
		
		m_worm->tState.write().bShoot = true;
		fLastShoot = tLX->currentTime;
		return true;
	}

	
	// If target is blocked by large amount of dirt, we can't shoot it
	// But we can use a clearing weapon :)
	if (nType & PX_DIRT)  {
		bDirect = false;
		if(diff.y < 0 && fabs(diff.y) > fabs(diff.x) && d-fDist > 40.0f)  {
			int w = AI_FindClearingWeapon();
			if (w >= 0 && AI_GetRockBetween(m_worm->vPos, cTrgPos) <= 3) {
				m_worm->iCurrentWeapon = w;
				if(AI_SetAim(cTrgPos)) {
					m_worm->tState.write().bShoot = true;
					return true;
				}
				else
					return false;
			}
		}
	}

	// In mortar game there must be enough of free cells around us
	if (iAiGameType == GAM_MORTARS)
		if (!(nType & PX_EMPTY) && fDist <= 25)
			return false;

    // Set the best weapon for the situation
    // If there is no good weapon, we can't shoot
    tLX->debug_float = d;
    int wpn = AI_GetBestWeapon(iAiGameType, d, bDirect, fDist);
    if(wpn < 0) {
        //notes << "bot: I could not find any useable weapon" << endl;
        return false;
    }

    m_worm->iCurrentWeapon = wpn;

    bool bAim = false;

	float alpha = 0;

	bool bShoot = false;

    // Aim in the right direction to account of weapon speed, gravity and worm velocity
	const weapon_t *weap = m_worm->getCurWeapon() ? m_worm->getCurWeapon()->weapon() : NULL;
	if(!weap) return false;
	if(weap->Proj.Proj)  {
		switch (weap->Proj.Proj->Hit.Type)  {
			//case PJ_NOTHING:
			//case PJ_CARVE:
			case PJ_DIRT:
			case PJ_GREENDIRT:
				//printf("hit_type is %i\n", weap->Proj.Proj->PlyHit.Type);
				// don't shoot this shit
				break;
			default:
				CVec direction = (w->getPos() - m_worm->vPos).Normalize();
				// speed of target in the direction (moving away from us)
				float targ_speed = direction.Scalar(w->getVelocity());
				float my_speed = direction.Scalar(m_worm->vVelocity);

				// Projectile speed (see CClient::ProcessShot for reference) - targ_speed
				float v = (float)weap->Proj.Speed/* *weap->Proj.Proj->Dampening */ + weap->Proj.SpeedVar*100.0f + my_speed;
				if(v < 0) {
					// we have high velocities, danger to shot...
					// if v<0, we would shoot in the wrong direction
					//printf("velocities(%f) too high...\n", v);
				/*	printf("  ProjSpeed = %f\n", (float)weap->Proj.Speed);
					printf("  Dampening = %f\n", (float)weap->Proj.Proj->Dampening);
					printf("  ProjSpeedVar = %f\n", weap->Proj.SpeedVar);
					printf("  my_speed = %f\n", my_speed);
					printf("  targ_speed = %f\n", targ_speed); */
					bAim = false;
					break;
				}

				// Distance
				float x = (cTrgPos.x-m_worm->vPos.get().x);
				float y = (m_worm->vPos.get().y-cTrgPos.y); // no PC-koord but real-world-koords



				// how long it takes for hitting the target
				float apriori_time = v ? sqrt(x*x + y*y) / v : 0;
				apriori_time *= 0.7f; // it's no direct line but a polynom, so this could give better results :)
				if(apriori_time < 0) {
					// target is faster than the projectile
					// shoot somewhere in the other direction
					notes << "bot: target is too fast! my speed: " << my_speed << ", trg speed: " << targ_speed << ", my abs speed: " << m_worm->vVelocity.get().GetLength() << ", trg abs speed: " << w->getVelocity().GetLength() << ", proj speed: " << (float)weap->Proj.Speed*weap->Proj.Proj->Dampening << "+" << (weap->Proj.SpeedVar*100.0f) << endl;

				} else { // apriori_time >= 0
					// where the target would be
					x += apriori_time*w->getVelocity().x;
					y -= apriori_time*w->getVelocity().y; // HINT: real-world-koords
				}

				// Gravity
				float g = 100;
				if(weap->Proj.Proj->UseCustomGravity)
					g = (float)weap->Proj.Proj->Gravity;

				proj_t *tmp = weap->Proj.Proj;
				while(tmp)  {
					if (tmp->UseCustomGravity)  {
						if (tmp->Gravity > g)
							g = (float)tmp->Gravity;
					} else
						if (g < 100.0f)
							g = 100.0f;

					// If there are any other projectiles, that are spawned with the main one, try their gravity
					if (tmp->Timer.Projectiles)  {
						if (tmp->Timer.Time >= 0.5f)
							break;
					}
					else if (tmp->Hit.Projectiles || tmp->PlyHit.Projectiles || tmp->Tch.Projectiles)
						break;

					// TODO: this is not correct anymore for newer gamescripts
					tmp = tmp->GeneralSpawnInfo.Proj;
				}
				g *= (float)cClient->getGameLobby()[FT_ProjGravityFactor];

				// Get the alpha
				bAim = AI_GetAimingAngle(v,g,x,y,&alpha);
				if (!bAim) {
					//printf("cannot calc the alpha, v=%f, g=%i, x=%f, y=%f\n", v,g,x,y);
					break;
				}

				// AI diff level
				// Don't shoot so exactly on easier skill levels
				static const int diff[4] = {13,8,3,0};

				if (tLX->currentTime-fLastRandomChange >= 0.5f)  {
					iRandomSpread = GetRandomInt(diff[iAiDiffLevel]) * SIGN(GetRandomNum());
					fLastRandomChange = tLX->currentTime;
				}

				alpha += iRandomSpread;

				// Can we hit the target?
				//printf("proj-speed: %f\n", v);
				if (g <= 10 || v >= 200)  {
					// we already have bDirect==false, if we have no direct free way
					bAim = bDirect;
				}

				if (!bAim)
					break;

				bShoot = true;

				float trg_dist2 = (cTrgPos - m_worm->vPos).GetLength2();
				if (trg_dist2 >= 2500)  {
					bAim = AI_SetAim(cTrgPos);
				} else if (trg_dist2 >= 100) {
					bAim = AI_SetAim(cTrgPos);
					if (iAiGameType != GAM_MORTARS) // Not in mortars - close shoot = suicide
						bShoot = true; // We are so close, that we almost cannot miss, just shoot
					break;
				} else { // Too close, get away!
					bAim = false;
					bShoot = false;
				}

				// Check if we can aim with this angle
				// HINT: we have to do it here because weaponCanHit requires already finished aiming
				if (bAim && g >= 10.0f && v <= 200)  {
					bShoot = bAim = weaponCanHit(g,v,m_worm->vPos+CVec(x,-y));
				}
				
				break;
			}

	// Not a projectile weapon
	} else {
		// Beam
		if (weap->Type == WPN_BEAM)  {
			int dist = (int)(cTrgPos - m_worm->vPos).GetLength();
			if (bDirect && weap->Bm.Length >= dist) // Check that the beam can reach the target
				bShoot = bAim = AI_SetAim(cTrgPos) || dist <= 25;
			else  {
				if (bDirect)
					AI_SetAim(cTrgPos); // Aim - when we come closer, we won't have to aim anymore
				bShoot = bAim = false;
			}
		}

	}

	//
	// If there's some lag or low FPS, don't shoot in the direction of our flight (avoid suicides)
	//

	// HINT: we don't need this, because we ensure above in the speed-calculation, that we have no problem
	// TODO: avoiding projectiles should not be done by not shooting but by changing MoveToTarget
	if(bAim) if (game.isClient())  {
		// Get the angle
		float ang = (float)atan2(m_worm->vVelocity.get().x, m_worm->vVelocity.get().y);
		ang = RAD2DEG(ang);
		if(m_worm->iFaceDirectionSide == DIR_LEFT)
			ang+=90;
		else
			ang = -ang + 90;

		// Cannot shoot
		if (fabs(m_worm->fAngle-ang) <= 30 && m_worm->vVelocity.get().GetLength2() >= 3600.0f && weap->Type != WPN_BEAM)  {
			if (weap->Type == WPN_PROJECTILE)  {
				if (weap->Proj.Proj->PlyHit.Damage > 0)
					return false;
			}
		}
	}

    if(!bAim)  {

  		// we cannot shoot here

		// TODO: do we need this?
		fBadAimTime += tLX->fDeltaTime;
		if((fBadAimTime) > 4) {
			if(game.gameMap()->IsEmptyForWorm(m_worm->pos() + CVec(0,-5)))
				m_worm->tState.write().bJump = true;
			fBadAimTime = 0;
		}

		fCanShootTime = 0;

        return bShoot;
	}

	// Reflexes :)
	// TODO: this doesn't work atm
	/*float diff[4] = {0.45f,0.35f,0.25f,0.0f};
	fCanShootTime += tLX->fDeltaTime;
	if (fCanShootTime <= diff[iAiDiffLevel])  {
		return false;
	}*/

	fBadAimTime = 0;
	vLastShootTargetPos = cTrgPos;

    // Shoot
	m_worm->tState.write().bShoot = true;
	fLastShoot = tLX->currentTime;
	return true;
}


///////////////////
// AI: Get the best weapon for the situation
// Returns weapon id or -1 if no weapon is suitable for the situation
int CWormBotInputHandler::AI_GetBestWeapon(int iGameMode, float fDistance, bool bDirect, float fTraceDist) {
	if(game.gameScript()->gusEngineUsed()) {
		// TODO: right weapon
		return 0;
	}

	// if we are to close to the target, don't selct any weapon (=> move away)
	/*if(fDistance < 5)
		return -1; */

	float diff[4] = {0.50f,0.30f,0.20f,0.12f};

    // We need to wait a certain time before we change weapon
    if( tLX->currentTime - fLastWeaponChange > diff[iAiDiffLevel] )
        fLastWeaponChange = tLX->currentTime;
    else
        return m_worm->iCurrentWeapon;

	// For rifles and mortars just get the first unreloaded weapon
	if (iAiGameType == GAM_RIFLES || iAiGameType == GAM_MORTARS)  {
		for (int i=0; i<5; i++)
			if (!m_worm->tWeapons[i].Reloading)
				return i;
		//printf("GAM_RIFLES|GAM_MORTARS: all weapons still are reloading...\n");
		return -1;
	}

    CVec    cTrgPos = AI_GetTargetPos();


	if (iAiGameType == GAM_100LT)  {
		// We're above the worm

		// If we are close enough, shoot the napalm
		if (m_worm->vPos.get().y <= cTrgPos.y && (m_worm->vPos-cTrgPos).GetLength2() < 10000.0f)  {
			if (traceWormLine(cTrgPos,m_worm->vPos) && !m_worm->tWeapons[1].Reloading) {
				CWorm* w = game.wormById(nAITargetWormId, false);
				if (w && w->CheckOnGround())
					return 1;
			}
		}



		float d = (m_worm->vPos-cTrgPos).GetLength();
		// We're close to the target
		if (d < 50.0f)  {
			// We see the target
			if(bDirect)  {
				// Super shotgun
				if (!m_worm->tWeapons[0].Reloading)
					return 0;

				// Chaingun
				if (!m_worm->tWeapons[4].Reloading)
					return 4;


				// Doomsday
				if (!m_worm->tWeapons[3].Reloading)
					return 3;

				// Let's try cannon
				if (!m_worm->tWeapons[2].Reloading)
				// Don't use cannon when we're on ninja rope, we will avoid suicides
					if (!m_worm->cNinjaRope.get().isAttached())  {
						m_worm->tState.write().bMove = false;  // Don't move, avoid suicides
						return 2;
					}
			}
			// We don't see the target
			else  {
				//notes << "bot: GAM_100LT: i think we should not shoot here" << endl;
				m_worm->tState.write().bJump = true; // Jump, we might get better position
				return -1;
			}
		}

		// Not close, not far
		if (d > 50.0f && d<=300.0f)  {
			if (bDirect)  {

				// Chaingun is the best weapon for this situation
				if (!m_worm->tWeapons[4].Reloading)  {
					return 4;
				}

				// Let's try cannon
				if (!m_worm->tWeapons[2].Reloading)
					// Don't use cannon when we're on ninja rope, we will avoid suicides
					if (!m_worm->cNinjaRope.get().isReleased())  {
						m_worm->tState.write().bMove = false;  // Don't move, avoid suicides
						return 2;
					}

				// Super Shotgun makes it sure
				if (!m_worm->tWeapons[0].Reloading)
					return 0;

				// As for almost last, try doomsday
				if (!m_worm->tWeapons[3].Reloading)
					// Don't use doomsday when we're on ninja rope, we will avoid suicides
					if (!m_worm->cNinjaRope.get().isAttached())  {
						m_worm->tState.write().bMove = false;  // Don't move, avoid suicides
						return 3;
					}
			} // End of direct shooting weaps

			//return -1;
		}

		// Quite far
		if (d > 300.0f && bDirect)  {

			// First try doomsday
			if (!m_worm->tWeapons[3].Reloading)  {
				// Don't use doomsday when we're on ninja rope, we will avoid suicides
				if (!m_worm->cNinjaRope.get().isAttached())  {
					m_worm->tState.write().bMove = false;  // Don't move, avoid suicides
					return 3;
				}
			}

			// Super Shotgun
			if (!m_worm->tWeapons[0].Reloading)
				return 0;

			// Chaingun
			if (!m_worm->tWeapons[4].Reloading)
				return 4;

			// Cannon, the worst possible for this
			if (!m_worm->tWeapons[2].Reloading)
				// Don't use cannon when we're on ninja rope, we will avoid suicides
				if (!m_worm->cNinjaRope.get().isReleased())  {
					// Aim a bit up
					// AI_SetAim(CVec(cTrgPos.x,cTrgPos.y+5.0f)); // don't do aiming here
					m_worm->tState.write().bMove = false;  // Don't move, avoid suicides
					return 2;
				}
		}

		//return -1;

	}

    /*
    0: "minigun"
	1: "shotgun"
	2: "chiquita bomb"
	3: "blaster"
	4: "big nuke"
    */


    /*

      Special firing cases

    */

    //
    // Case 1: The target is on the bottom of the level, a perfect spot to lob an indirect weapon
    //
    if(cTrgPos.y > game.gameMap()->GetHeight()-50 && fDistance < 200) {
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading)
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_PROJECTILE)
					if (m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type == PJ_EXPLODE)
						return (int)i;
    }



    /*

      Direct firing weapons

    */

    //
    // If we're close, use some beam or projectile weapon
    //
    if(fDistance < 100 && bDirect) {
        // First try beam
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading)
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_BEAM)
					return (int)i;

		// If beam not available, try projectile
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading)
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_PROJECTILE)
					if( m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type != PJ_DIRT
					&& m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type != PJ_GREENDIRT)
					//if (tWeapons[i].Weapon->Proj.Proj->Type == PRJ_PIXEL)
					return (int)i;

 		// don't return here, try selection by other, not optimal fitting cases
   }


    //
    // If we're at a medium distance, use any weapon, but prefer the exact ones
    //
    if(fDistance < 150 && bDirect) {

		// First try beam
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading)
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_BEAM)
					return (int)i;

		// If beam not available, try projectile
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading)
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_PROJECTILE)
					if( m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type != PJ_DIRT
					&& m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type != PJ_GREENDIRT)
					/*if (tWeapons[i].Weapon->Proj.Proj->Type == PRJ_PIXEL || tWeapons[i].Weapon->Proj.Proj->Hit.Type == PJ_BOUNCE)*/
						return (int)i;

		// don't return here, try selection by other, not optimal fitting cases
    }


    //
    // Any greater distance for direct firing uses a projectile weapon first
    //
    if(bDirect) {
		// First try projectile
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading)
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_PROJECTILE)
					if( m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type != PJ_DIRT
					&& m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type != PJ_GREENDIRT)
						return (int)i;

		// If projectile not available, try beam
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading)
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_BEAM)
					return (int)i;

		// If everything fails, try some random weapons
		for (size_t i=0; i<m_worm->tWeapons.size(); i++) {
			int num = GetRandomInt((int)m_worm->tWeapons.size()-1);
			if (!m_worm->tWeapons[num].Reloading && m_worm->tWeapons[num].weapon())
				return num;
		}

		//return -1;
    }


    //
    // Indirect firing weapons
    //


    // If we're above the target, try any special weapon, for Liero mod try napalm
    // BUT only if our health is looking good
    // AND if there is no rock/dirt nearby
	if(fDistance > 190 && m_worm->health > 25 && fTraceDist > 0.5f && (cTrgPos.y-20) > m_worm->vPos.get().y ) {
        if (!AI_CheckFreeCells(5)) {
			//notes << "bot: we should not shoot because of the hints everywhere" << endl;
			return -1;
        }

		// try projectile weapons
		for (size_t i=0; i<m_worm->tWeapons.size(); i++)
			if (!m_worm->tWeapons[i].Reloading && m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_PROJECTILE)
				if (m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type == PJ_EXPLODE || m_worm->tWeapons[i].weapon()->Proj.Proj->Hit.Type == PJ_BOUNCE)
					return (int)i;

    }


    //
    // Last resort
    //

    // Shoot a beam (we cant suicide with that)
	for (size_t i=0; i<m_worm->tWeapons.size(); i++)
		if (!m_worm->tWeapons[i].Reloading && m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Type == WPN_BEAM)
			return (int)i;

	// If everything fails, try some random weapons
	for (size_t i=0; i<m_worm->tWeapons.size(); i++) {
		int num = GetRandomInt((int)m_worm->tWeapons.size()-1);
		if (!m_worm->tWeapons[num].Reloading && m_worm->tWeapons[num].weapon())
			return num;
	}

	// If everything fails, try all weapons
	for (size_t i=0; i<m_worm->tWeapons.size(); i++)
		if (!m_worm->tWeapons[i].Reloading && m_worm->tWeapons[i].weapon())
			return (int)i;

    return -1;
}




///////////////////
// Trace a line from this worm to the target
// Returns the distance the trace went
int CWorm::traceLine(CVec target, float *fDist, int *nType, int divs)  {
	int res =  traceLine(target, vPos, nType, divs);
	if (fDist && res) *fDist = (float)((int)(vPos - target).GetLength() / res);
	return res;
}

int CWorm::traceLine(CVec target, CVec start, int *nType, int divs, uchar checkflag)
{
    if( game.gameMap() == NULL ) {
		errors << "CWorm::traceLine: map unset" << endl;
		return 0;
	}

    // Trace a line from the worm to length or until it hits something
	CVec pos = start;
	CVec dir = target-pos;
    int nTotalLength = (int)NormalizeVector(&dir);

	int divisions = divs;			// How many pixels we go through each check (more = slower)

	if( nTotalLength < divisions)
		divisions = nTotalLength;

	if (nType)
		*nType = PX_EMPTY;

	// Make sure we have at least 1 division
	divisions = MAX(divisions,1);

	int i;
	for(i=0; i<nTotalLength; i+=divisions) {
		uchar px = game.gameMap()->GetPixelFlag( (int)pos.x, (int)pos.y );
		//game.gameMap()->PutImagePixel((int)pos.x, (int)pos.y, Color(255,0,0));

        if(!(px & checkflag)) {
			if (nType)
				*nType = px;
            return i;
        }

        pos = pos + dir * (float)divisions;
    }

    return nTotalLength;
}

// Finds the nearest free cell in the map and returns coordinates of its midpoint
CVec CWormBotInputHandler::AI_FindClosestFreeCell(CVec vPoint)
{
	return game.gameMap()->FindSpotCloseToPos(vPoint);
}

/////////////////////
// Trace the line for the current weapon
int CWormBotInputHandler::traceWeaponLine(CVec target, float *fDist, int *nType)
{
	if( game.gameMap() == NULL) {
		errors << "AI:traceWeaponLine: map == NULL" << endl;
		return 0;
	}

	// Trace a line from the worm to length or until it hits something
	CVec    pos = m_worm->vPos;
	CVec    dir = target-pos;
	int     nTotalLength = (int)NormalizeVector(&dir);

	int first_division = 7;	// How many pixels we go through first check (we can shoot through walls)
	int divisions = 5;		// How many pixels we go through each check (more = slower)

	if(!game.gameScript()->gusEngineUsed()) {

		if(!m_worm->getCurWeapon() || !m_worm->getCurWeapon()->weapon())
			return 0;
		

		//
		// Predefined divisions
		//

		// Beam
		if (m_worm->tWeapons[m_worm->iCurrentWeapon].weapon()->Type == WPN_BEAM)  {
			first_division = 1;
			divisions = 1;
		}

		// Rifles
		else if (iAiGameType == GAM_RIFLES)  {
			first_division = 10;
			divisions = 6;
		}

		// Mortars
		else if (iAiGameType == GAM_MORTARS)  {
			first_division = 7;
			divisions = 2;
		}

		// 100lt
		else if (iAiGameType == GAM_100LT)  {
			first_division = 6;
			divisions = 3;
		}

	}

	// Add the worm thickness
	first_division += 5;

	// Check
	if( nTotalLength < divisions)
		divisions = nTotalLength;

    *nType = PX_EMPTY;

	// Make sure we have at least 1 division
	divisions = MAX(divisions,1);
	first_division = MAX(first_division,1);

	// Check that we don't hit any teammate
	std::vector<CVec> WormsPos;
	if (cClient && cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->generalGameType == GMT_TEAMS)  {
		for_each_iterator(CWorm*, w, game.aliveWorms()) {
			if (w->get()->getTeam() == m_worm->iTeam && w->get()->getID() != m_worm->iID)
				WormsPos.push_back(w->get()->getPos());
		}
	}


	// Trace the line
	int divs = first_division;
	int i;
	for(i=0; i<nTotalLength; i+=divs) {
		uchar px = game.gameMap()->GetPixelFlag( (int)pos.x, (int)pos.y );

		if (i>first_division)  // we aren't close to a wall, so we can shoot through only thin wall
			divs = divisions;

		// Dirt or rock
		if((px & PX_DIRT) || (px & PX_ROCK)) {
        	if(nTotalLength != 0)
            	*fDist = (float)i / (float)nTotalLength;
            else
            	*fDist = 0;
			*nType = px;
			return i;
		}

		// Friendly worm
		for (size_t j=0;j<WormsPos.size();j++) {
			if ((pos-WormsPos[j]).GetLength2() < 400.0f)  {
				if(nTotalLength != 0)
					*fDist = (float)i / (float)nTotalLength;
				else
					*fDist = 0;
				*nType = *nType | PX_WORM;
				return i;
			}
		}

		pos += dir * (float)divs;
	}

	// Full length
	if(nTotalLength != 0)
		*fDist = (float)i / (float)nTotalLength;
	else
		*fDist = 0;
	return nTotalLength;
}




class set_col_and_break {
public:
	CVec start;
	CVec* collision;
	bool hit;

	set_col_and_break(CVec st, CVec* col) : start(st), collision(col), hit(false) {}
	bool operator()(int x, int y) {
		hit = true;
		if(collision && (*collision - start).GetLength2() > (CVec((float)x,(float)y) - start).GetLength2()) {
			collision->x = (float)x;
			collision->y = (float)y;
		}
		return false;
	}
};


////////////////////
// Trace the line with worm width
int traceWormLine(CVec target, CVec start, CVec* collision)
{
	// At least three, else it goes through walls in jukke
	static const unsigned short wormsize = 3;

	if(collision) {
		collision->x = target.x;
		collision->y = target.y;
	}

	CVec dir = CVec(target.y-start.y,start.x-target.x); // rotate clockwise by 90 deg
	NormalizeVector(&dir);
	set_col_and_break action = set_col_and_break(start - dir*(wormsize-1)/2, collision);
	target -= dir*(wormsize-1)/2;
	for(unsigned short i = 0; i < wormsize; i++, action.start += dir, target += dir)
		fastTraceLine(target, action.start, (uchar)PX_ROCK, action);

	return !action.hit;

}

////////////////////////
// Checks if there is enough free cells around us to shoot
bool CWormBotInputHandler::AI_CheckFreeCells(int Num)
{
	// TODO: it seems this function is used much too radical and I am not exactly sure what it should calculate
	// So this is a bad hack for now
	return true;
	// TODO: is there any difference?
	return AI_IsInAir(m_worm->pos(), Num);
}


////////////////////
// Creates the path
int CWormBotInputHandler::AI_CreatePath(bool force_break)
{

	CVec trg = AI_GetTargetPos();
	if (nAITargetWormId >= 0 && nAITargetType == AIT_WORM)
		trg = AI_FindShootingSpot();  // If our target is an enemy, go to the best spot for shooting

	if(force_break) {
		bPathFinished = false;
		fSearchStartTime = tLX->currentTime;
		pathSearcher->restartThreadSearch(m_worm->vPos.get(), trg);

		return false;
	}

	// bPathFinished also implicates, that there is currently no search
	// if bPathFinished is set in the wrong way, this will result in multithreading errors!
	// TODO: something has to be changed here, because the name bPathFinished doesn't indicates this
	if(!bPathFinished) {
		// have we finished a current search?
		if(pathSearcher->isReady()) {

			bPathFinished = true;
			NEW_ai_node_t* res = pathSearcher->resultedPath();

			// have we found something?
			if(res) {
				// in every case, the nodes of the current path are not handled by pathSearcher
				// so we have to delete it here
				delete_ai_nodes(NEW_psPath);

				NEW_psPath = res;
				NEW_psLastNode = get_last_ai_node(NEW_psPath);
				NEW_psCurrentNode = NEW_psPath;

				// prevent it from deleting the current path (it will be deleted, when the new path is found)
				pathSearcher->removePathFromList(NEW_psPath);


				fLastCreated = tLX->currentTime;

				return true;
			}
			// we don't find anything, so don't return here, but start a new search

		} else { // the searcher is still searching ...

			// restart search in some cases
			if(!pathSearcher->shouldRestartThread() && (tLX->currentTime - fSearchStartTime >= 5.0f || !traceWormLine(m_worm->vPos, pathSearcher->start) || !traceWormLine(trg, pathSearcher->target))) {
				fSearchStartTime = tLX->currentTime;
				pathSearcher->restartThreadSearch(m_worm->vPos.get(), trg);
			}

			return false;
		}
	}

	// don't start a new search, if the current end-node still has direct access to it
	// however, we have to have access somewhere to the path
	if(NEW_psLastNode && traceWormLine(CVec(NEW_psLastNode->fX, NEW_psLastNode->fY), trg)) {
		for(NEW_ai_node_t* node = NEW_psPath; node; node = node->psNext)
			if(traceWormLine(CVec(node->fX,node->fY),m_worm->vPos,NULL)) {
				NEW_psCurrentNode = node;
				NEW_psLastNode->psNext = createNewAiNode(trg.x, trg.y, NULL, NEW_psLastNode);
				NEW_psLastNode = NEW_psLastNode->psNext;
				return true;
			}
	}

	// Don't create the path so often!
	if (tLX->currentTime - fLastCreated <= 0.5f)  {
		return NEW_psPath != NULL;
	}

	// if we are here, we want to start a new search
	// the searcher-thread is currently ready
	bPathFinished = false;

	// start a new search
	fSearchStartTime = tLX->currentTime;
	pathSearcher->target.x = (int)trg.x;
	pathSearcher->target.y = (int)trg.y;
	pathSearcher->start = VectorD2<int>(m_worm->vPos.get());
	pathSearcher->startThreadSearch();

	return false;
}


int CWormBotInputHandler::AI_GetRockBetween(CVec pos, CVec trg)
{
    assert( game.gameMap() != NULL );

	int result = 0;

    // Trace a line from the worm to the target
	CVec    dir = trg-pos;
    int     nTotalLength = (int)NormalizeVector(&dir);

	const int divisions = 4;			// How many pixels we go through each check (less = slower)

	int i;
	uchar px = PX_EMPTY;
	for(i=0; i<nTotalLength; i+=divisions) {
		px = game.gameMap()->GetPixelFlag( (int)pos.x, (int)pos.y );
		//game.gameMap()->PutImagePixel((int)pos.x, (int)pos.y, Color(255,0,0));

        if (px & PX_ROCK)
			result++;

        pos = pos + dir * (float)divisions;
    }

	return result;
}

/*CVec NEW_AI_FindBestSpot(CVec trg, CVec pos)
{
	// Get the midpoint
	CVec middle = CVec((pos.x+trg.x)/2,(pos.y+trg.y)/2);

	float a = CalculateDistance(middle,pos);
	float b = a;
	float step = DEG2RAD(20);
	int min = 99999999;
	CVec result = CVec(0,0);

	// We make a circle and check from the points the way with least rock
	float i;
	float x,y;
	for (;b > 10.0f; b-=b/2)
		for (i=0;i<2*PI; i+=step)  {
			x = a*sinf(2*PI*i)+middle.x;
			y = b*cosf(2*PI*i)+middle.y;
			CVec point = CVec(x,y);
			if (game.gameMap()->GetPixelFlag( (int)pos.x, (int)pos.y ) & PX_ROCK)
				continue;

			int rock_pixels = GetRockBetween(point,pos,game.gameMap())+GetRockBetween(point,trg,game.gameMap());
			if (rock_pixels < min)  {
				min = rock_pixels;
				result.x=(point.x);
				result.y=(point.y);

				if (!min)
					return result;
			}
		}

	return result;
}*/


//////////////////
// Finds the closest free spot, looking only in one direction
CVec CWormBotInputHandler::AI_FindClosestFreeSpotDir(CVec vPoint, CVec vDirection, int Direction = -1)
{
#ifdef _AI_DEBUG
//	SmartPointer<SDL_Surface> & bmpDest = game.gameMap()->GetDebugImage();
#endif

	NormalizeVector(&vDirection);
	//CVec vDev = CVec(vDirection.x*5,vDirection.y*5);
	CVec vDev = CVec(0,0);

	int i;
	int emptyPixels = 0;
	CVec pos = vPoint+vDev;
	int firstClosest = 9999;
	int secondClosest = 9999;
	CVec rememberPos1 = CVec(0,0);
	CVec rememberPos2 = CVec(0,0);

	for(i=0; 1; i++) {
		uchar px = game.gameMap()->GetPixelFlag( (int)pos.x, (int)pos.y );

		// Empty pixel? Add it to the count
		if(!(px & PX_ROCK)) {
#ifdef _AI_DEBUG
			//PutPixel(bmpDest,pos.x*2,pos.y*2,Color(255,255,0));
#endif
			emptyPixels++;
		}
		// Rock pixel? This spot isn't good
		else {
#ifdef _AI_DEBUG
			//PutPixel(bmpDest,pos.x*2,pos.y*2,Color(255,0,0));
#endif
			if (emptyPixels >= 10)
				break;
			emptyPixels = 0;
			rememberPos1.x=(0);
			rememberPos1.y=(0);
		}

		// Good spot
		if (emptyPixels >= 10)  {
			firstClosest = i-emptyPixels;
			if (emptyPixels >= 30)
				break;
			rememberPos1.x=(rememberPos1.x+vDirection.x);
			rememberPos1.y=(rememberPos1.y+vDirection.y);
		}

		if (emptyPixels == 5)  {
			rememberPos1.x=(pos.x);
			rememberPos1.y=(pos.y);
		}

		pos = pos + vDirection;
		// Clipping
		if (pos.x > game.gameMap()->GetWidth() || pos.x < 0)
			break;
		if (pos.y > game.gameMap()->GetHeight() || pos.y < 0)
			break;
	}

	if (firstClosest != 9999 && Direction == DIR_LEFT)
		return rememberPos1;

	emptyPixels = 0;
	vDirection.y=(-vDirection.y);
	vDirection.x=(-vDirection.x);
	vDev.x=(-vDev.x);
	vDev.y=(-vDev.y);
	pos = vPoint+vDev;

	for(i=0; 1; i++) {
		uchar px = game.gameMap()->GetPixelFlag( (int)pos.x, (int)pos.y );

		// Empty pixel? Add it to the count
		if(!(px & PX_ROCK)) {
#ifdef _AI_DEBUG
			//PutPixel(bmpDest,pos.x*2,pos.y*2,Color(255,255,0));
#endif
			emptyPixels++;
		}
		// Rock pixel? This spot isn't good
		else {
#ifdef _AI_DEBUG
			//PutPixel(bmpDest,pos.x*2,pos.y*2,Color(255,0,0));
#endif
			if (emptyPixels >= 10)
				break;

			rememberPos2.x=(0);
			rememberPos2.y=(0);
			emptyPixels = 0;
		}

		// Good spot
		if (emptyPixels > 10)  {
			secondClosest = i-emptyPixels;
			if (emptyPixels >= 30)
				break;
			rememberPos2.x=(rememberPos2.x+vDirection.x);
			rememberPos2.y=(rememberPos2.y+vDirection.y);
		}

		// Remember this special position (in the middle of possible free spot)
		if (emptyPixels == 5)  {
			rememberPos2.x=(pos.x);
			rememberPos2.y=(pos.y);
		}

		pos = pos + vDirection;
		// Clipping
		if (pos.x > game.gameMap()->GetWidth() || pos.x < 0)
			break;
		if (pos.y > game.gameMap()->GetHeight() || pos.y < 0)
			break;
	}

	if (secondClosest != 9999 && Direction == DIR_RIGHT)
		return rememberPos2;

	// In what direction was the closest spot?
	if (firstClosest < secondClosest)
		return rememberPos1;
	else
		return rememberPos2;
}


#ifdef _AI_DEBUG
///////////////////
// Draw the AI path
void CWormBotInputHandler::AI_DrawPath()
{
	if (!NEW_psPath)
		return;

	SmartPointer<SDL_Surface> bmpDest = game.gameMap()->GetDebugImage();
	if (!bmpDest.get())
		return;

	const Color NodeColour = m_worm->getGameColour();
	const Color HighColour = Color(255, 0, 0);
	const Color LineColour = tLX->clWhite;

	// Go down the path
	NEW_ai_node_t *node = NEW_psCurrentNode;
	int node_x = 0;
	int node_y = 0;
	for (;node;node=node->psNext)  {

		// Get the node position
		node_x = Round(node->fX*2);
		node_y = Round(node->fY*2);

		// Clipping
		if (node_x-4 < 0 || node_x+4 > bmpDest.get()->w)
			continue;
		if (node_y-4 < 0 || node_y+4 > bmpDest.get()->h)
			continue;

		// Draw the node
		if (node == NEW_psCurrentNode)
			DrawRectFill(bmpDest.get(),node_x-4,node_y-4,node_x+4,node_y+4,HighColour);
		else
			DrawRectFill(bmpDest.get(),node_x-4,node_y-4,node_x+4,node_y+4,NodeColour);

		// Draw the line
		if (node->psNext)
			DrawLine(bmpDest.get(),MIN(Round(node->psNext->fX*2),bmpDest.get()->w),MIN(Round(node->psNext->fY*2),bmpDest.get()->h),node_x,node_y,LineColour);
	}

}
#endif


class bestropespot_collision_action {
public:
	CWorm* worm;
	CVec aimDir;
	CVec target, best;
	float best_value;

	bestropespot_collision_action(CWorm* w, CVec t) : worm(w), target(t), best_value(-1) {
        target.y -= 30.0f; // a bit higher is always better
		aimDir = worm->getFaceDirection();
	}

	bool operator()(int x, int y) {

		CVec suggestion((float)x, (float)y);

		float len = (worm->getPos() - suggestion).GetLength();
		if(len < 30.0f) // just ignore too close spots
			return false;
		
		if(worm->getPos().y > target.y && (y > worm->getPos().y - 10))
			// suggestion is lower than worm but we want to get higher -> ignore
			return false;
		
		float trg_dist = (suggestion - target).GetLength();
		trg_dist = (trg_dist != 0) ? (1.0f / trg_dist) : 999999999999999.0f; // the closer we are the better it is
		float angle_dif = (aimDir - suggestion / suggestion.GetLength()).GetLength();
		angle_dif = (angle_dif != 0) ? (1.0f / angle_dif) : 999999999999999.0f; // the closer the angle the better it is
		
		len = -len * (len - 200.0f); // 0 is bad and everything behind 100.0f also
		if(len < 0) len = 0.0f;

		// HINT: these constant multiplicators are the critical values in the calculation
		float value = (float)(trg_dist * 100000.0f + angle_dif * 10000.0f + len * 0.001f);
		//printf("value: %f, %f, %f, %f\n", trg_dist, angle_dif, len, value);

		// FIX: if we want to go up, then ignore angle and len
		if(worm->getPos().y - target.y > 40.0f)
			value = trg_dist;

		if(best_value < value) {
			best_value = value;
			best = CVec((float)x, (float)y);
		}

		return false;
	}
};

/////////////////////////
// Finds the best spot to shoot rope to if we want to get to trg
CVec CWormBotInputHandler::AI_GetBestRopeSpot(CVec trg)
{
	
	
	// Get the direction angle
	CVec dir = trg - m_worm->vPos;
	if(dir.GetLength2() < 0.001)
		// we are too close
		return trg;

	const float ropeMaxLength = (float)(int)cClient->getGameLobby()[FT_RopeMaxLength];
	dir *= ropeMaxLength / dir.GetLength();
	dir = CVec(-dir.y, dir.x); // rotate reverse-clockwise by 90 deg

	// Variables
	float step = 0.05f*(float)PI;
	float ang = 0;

	SquareMatrix<float> step_m = SquareMatrix<float>::RotateMatrix(-step);
	bestropespot_collision_action action(m_worm, trg);

	for(ang=0; ang<(float)PI; dir=step_m(dir), ang+=step) {
		fastTraceLine(m_worm->vPos+dir, m_worm->vPos, PX_ROCK|PX_DIRT, action);
	}

	if(action.best_value < 0) // we don't find any spot
		return trg;
	else
		return action.best;
}


///////////////
// Returns true if the point has the specified amount of air around itself
// HINT: not always the opposite to CheckOnGround because it checks also left/right/top side
// At least area_a cells around have to be free for this function to return true
bool CWormBotInputHandler::AI_IsInAir(CVec pos, int area_a)
{
	if(pos.x < 0 || pos.y < 0 || pos.x >= game.gameMap()->GetWidth() || pos.y >= game.gameMap()->GetHeight())
		return false;

	const long w = area_a * 15;
	const long h = w;
	const long x = (long)pos.x - w/2;
	const long y = (long)pos.y - h/2;
	
	CMap::PixelFlagAccess flags(game.gameMap());
	return flags.checkArea_AllHaveNot<PX_ROCK|PX_DIRT>(x,y,x+w,y+h);
}


///////////////////
// AI carving
void CWormBotInputHandler::AI_Carve()
{
	// Don't carve too fast
	if (tLX->currentTime - m_worm->fLastCarve > 0.2f)  {
		m_worm->fLastCarve = tLX->currentTime;
		m_worm->tState.write().bCarve = true; // Carve
	}
	else  {
		m_worm->tState.write().bCarve = false;
	}
}


static float estimateYDiffAfterJump(float dt) {
	const float jumpForce = cClient->getGameLobby()[FT_WormJumpForce];
	//const float drag = w->getGameScript()->getWorm()->AirFriction; // ignoring for now
	const float grav = cClient->getGameLobby()[FT_WormJumpForce];

	return grav*dt*dt*0.5f + jumpForce*dt;
}

static bool isJumpingGivingDisadvantage(NEW_ai_node_t* node, CWorm* w) {
	if(!node) return false;
	
	float dy = estimateYDiffAfterJump(0.3f);
	if(!traceWormLine(CVec(node->fX, node->fY), w->getPos() + CVec(0,dy)))
		return true;
	
	return false;
}

static float estimateXDiffAfterMove(CWorm* w, float dt) {
	float speed = w->isOnGround() ? (float)cClient->getGameLobby()[FT_WormGroundSpeed] : (float)cClient->getGameLobby()[FT_WormAirSpeed];
	if(w->getFaceDirectionSide() == DIR_LEFT) speed = -speed;
	
	return CLAMP(w->getVelocity().x + speed * 90.0f, -30.0f, 30.0f) * dt;
}

static bool isMoveGivingDisadvantage(CVec target, CWorm* w) {
	float dx = estimateXDiffAfterMove(w, 0.3f);
	
	if(!traceWormLine(target, w->getPos() + CVec(dx,0)))
		return true;
	
	return false;
}

///////////////////
// AI jumping, returns true if we really jumped
bool CWormBotInputHandler::AI_Jump()
{
	// Don't jump so often
	if ((GetTime() - fLastJump).seconds() > 0.3f && (m_worm->bOnGround || m_worm->canAirJump()) && !isJumpingGivingDisadvantage(NEW_psCurrentNode, m_worm))  {
		fLastJump = GetTime();
		m_worm->tState.write().bJump = true;
	}
	// TODO: why this? we should have reset it anyway. and multiple calls to AI_Jump should not make it false again
	/*else  {
		m_worm->tState.bJump = false;
	}*/

	return m_worm->tState.get().bJump;
}

/////////////////////
// Move to the target
void CWormBotInputHandler::AI_MoveToTarget()
{
//	printf("Moving to target");

	worm_state_t *ws = &m_worm->tState.write();

	// No target?
	if (nAITargetType == AIT_NONE || (nAITargetType == AIT_WORM && game.wormById(nAITargetWormId, false) == NULL))  {
		nAIState = AI_THINK;
		return;
	}

	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_RACE || cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_TEAMRACE) {		
		int t = m_worm->getID();
		if(cClient->isTeamGame()) t = m_worm->getTeam();
		
		Flag* flag = cClient->flagInfo()->getFlag(t);
		if(!flag) { // strange
			nAIState = AI_THINK;
			return;
		}
		
		if((cPosTarget - flag->spawnPoint.pos).GetLength2() > 10.0f) {
			nAIState = AI_THINK;
			return;
		}
	}
	
    // Clear the state
	ws->bMove = false;
	ws->bShoot = false;
	ws->bJump = false;

	// If the rope hook is attached, increase the attached time
	if (m_worm->cNinjaRope.get().isAttached())
		fRopeAttachedTime += tLX->fDeltaTime;
	else
		fRopeAttachedTime = 0;

	// release the rope if we used it to long
	if (fRopeAttachedTime > 5.0f) {
		m_worm->cNinjaRope.write().Clear();
		fRopeAttachedTime = 0;
	}

	if(m_worm->cNinjaRope.get().isShooting() && !m_worm->cNinjaRope.get().isAttached())
		fRopeHookFallingTime += tLX->fDeltaTime;
	else
		fRopeHookFallingTime = 0;

	if (fRopeHookFallingTime >= 2.0f)  {
		// Release & walk
		m_worm->cNinjaRope.write().Clear();
//        ws->bMove = true;
		fRopeHookFallingTime = 0;
	}

    cPosTarget = AI_GetTargetPos();
	if (nAITargetType == AIT_WORM && nAITargetWormId >= 0)
		cPosTarget = AI_FindShootingSpot();

	bool canShoot = true;
	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode && !cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode->Shoot(m_worm))
		canShoot = false;
	
	// If we just shot some mortars, release the rope if it pushes us in the direction of the shots
	// and move away!
	if (canShoot && iAiGameType == GAM_MORTARS)  {
		if (SIGN(m_worm->vVelocity.get().x) == SIGN(vLastShootTargetPos.x - m_worm->vPos.get().x) && tLX->currentTime - fLastShoot >= 0.2f && tLX->currentTime - fLastShoot <= 1.0f)  {
			if (m_worm->cNinjaRope.get().isAttached() && SIGN(m_worm->cNinjaRope.get().GetForce().x) == SIGN(vLastShootTargetPos.x - m_worm->vPos.get().x))
				m_worm->cNinjaRope.write().Clear();

			m_worm->iFaceDirectionSide = m_worm->vPos.get().x < vLastShootTargetPos.x ? DIR_LEFT : DIR_RIGHT;
			ws->bMove = true;
		}
	}

	if (canShoot && (iAiGameType == GAM_MORTARS || iAiGameType == GAM_100LT))  {
		// If there's some worm in sight and we are on ground, jump!
		if (m_worm->bOnGround)  {
			for_each_iterator(CWorm*, w, game.aliveWorms()) {
				if (w->get()->getID() != m_worm->iID)  {
					if ((m_worm->vPos - w->get()->getPos()).GetLength2() <= 2500)  {
						float dist;
						int type;
						m_worm->traceLine(w->get()->getPos(), &dist, &type);
						if (type & PX_EMPTY)
							AI_Jump();
					}
				}
			}
		}
	}

	// Don't do anything crazy a while after shooting
	if (canShoot && iAiGameType == GAM_MORTARS)  {
		if (tLX->currentTime - fLastShoot <= 1.0f)
			return;
	}

    // If we're stuck, just get out of wherever we are
    if(bStuck) {
//		printf("Stucked");

        ws->bMove = true;
		AI_Jump();

		if (tLX->currentTime-fLastFace >= 0.5f)  {
			m_worm->iFaceDirectionSide = OppositeDir((DIR_TYPE)(int)m_worm->iFaceDirectionSide);
			fLastFace = tLX->currentTime;
		}
		
        if(tLX->currentTime - fStuckPause > 2.0f)
            bStuck = false;

        // return here, because we force this stuck-action
        // if we would not return here, the stuck-check makes no sense
        return;
    }

    bool fireNinja = false;

	/*
		Prevent injuries! If any of the projectiles around is heading to us, try to get away from it
	*/
	// TODO: this doesn't work that good atm; so it's better to ignore it at all than to go away in a situation where shooting would be better
#if 0
	if (false)  {
		// TODO: improve this

		// just temp here as I removed it globally, we should try to get a collection of projectiles here automatically (not only one)
		CProjectile* psHeadingProjectile = NULL;

		// Go away from the projectile
		if (tLX->currentTime-fLastFace >= 0.5f)  {
			if (psHeadingProjectile->getVelocity().x > 0)
				m_worm->iFaceDirectionSide = DIR_LEFT;  // Move in the opposite direction
			else
				m_worm->iFaceDirectionSide = DIR_RIGHT;
			fLastFace = tLX->currentTime;
		}
		ws->bMove = true;


		// If we're on ground, jump
		AI_Jump();

		// Release any previous rope
		if (fRopeAttachedTime >= 1.5f)
			m_worm->cNinjaRope.write().Clear();

		// Shoot the rope
		fireNinja = true;

		/*
		// We want to move away
		CVec desired_dir = -psHeadingProjectile->getVelocity();

		// Choose some point and find the best rope spot to it
		desired_dir = desired_dir.Normalize() * 40.0f;
		CVec cAimPos = AI_GetBestRopeSpot(m_worm->vPos+desired_dir);
		
		// Aim it
		// TODO: why isn't this used any more?
		fireNinja = AI_SetAim(cAimPos);

		if (fireNinja)
			fireNinja = psHeadingProjectile->getVelocity().GetLength() > 50.0f;

		if (fireNinja)  {

			// Get the direction
			CVec dir;
			dir.x=( cosf(fAngle * (PI/180)) );
			dir.y=( sinf(fAngle * (PI/180)) );
			if(iFaceDirectionSide == DIR_LEFT)
				dir.x=(-dir.x);

			// Shoot it
			cNinjaRope.Shoot(vPos,dir);
		}*/

		return;
	}
#endif
	// TODO: in general, move away from projectiles

	// prevent suicides
	if (canShoot && iAiGameType == GAM_MORTARS)  {
		if (tLX->currentTime - fLastShoot <= 0.2f)  {
			if (fRopeAttachedTime >= 0.1f)
				m_worm->cNinjaRope.write().Clear();
			return;
		}
	}


    /*
      Move through the path.
      We have a current node that we must get to. If we go onto the node, we go to the next node, and so on.
    */

	//return; // Uncomment this when you don't want the AI to move

    if(NEW_psPath == NULL || NEW_psCurrentNode == NULL) {
		nAIState = AI_THINK;
        // If we don't have a path, resort to simpler AI methods
        AI_SimpleMove(nAITargetWormId >= 0);
		//printf("Pathfinding problem 2; ");
        return;
    }

//	printf("We should move now...");

	if ((CVec(NEW_psCurrentNode->fX, NEW_psCurrentNode->fY) - m_worm->vPos).GetLength2() <= 100)  {
		if (NEW_psCurrentNode->psNext)
			NEW_psCurrentNode = NEW_psCurrentNode->psNext;
	}

	// If some of the next nodes is closer than the current one, just skip to it
	NEW_ai_node_t *next_node = NEW_psCurrentNode->psNext;
	bool newnode = false;
	while(next_node)  {
		if(traceWormLine(CVec(next_node->fX, next_node->fY), m_worm->vPos))  {
			NEW_psCurrentNode = next_node;
			newnode = true;
		}
		next_node = next_node->psNext;
	}
	if(!newnode) {
		// check, if we have a direct connection to the current node
		// else, choose some last node
		// this will work and is in many cases the last chance
		if (tLX->currentTime - fLastGoBack >= 1)  {
			for(next_node = NEW_psCurrentNode; next_node; next_node = next_node->psPrev) {
				if(traceWormLine(CVec(next_node->fX, next_node->fY), m_worm->vPos))  {
					if(NEW_psCurrentNode != next_node) {
						NEW_psCurrentNode = next_node;
						fLastGoBack = tLX->currentTime;
 						newnode = true;
 					}
					goto find_one_visible_node;
				}
			}
		}

		// we currently have no visible node
		if (!NEW_psCurrentNode)  {
			//notes << "AI: no current node" << endl;
			nAIState = AI_THINK;
			AI_SimpleMove(nAITargetWormId >= 0);
			return;
		}
	}
find_one_visible_node:


	// Get the target node position
    CVec nodePos(NEW_psCurrentNode->fX, NEW_psCurrentNode->fY);


	// release rope, if it forces us to the wrong direction
	if(m_worm->cNinjaRope.get().isAttached() && (m_worm->cNinjaRope.get().GetForce().Normalize() + m_worm->vPos - nodePos).GetLength2() > (m_worm->vPos - nodePos).GetLength2()) {
		m_worm->cNinjaRope.write().Clear();
		fRopeAttachedTime = 0;
	}


	bool we_see_the_target;
	{
		float dist; int type;
		traceWeaponLine(cPosTarget, &dist, &type);
		we_see_the_target = (type & PX_EMPTY);

		/*
		  For rifle games: it's not clever when we go to the battle with non-reloaded weapons
		  If we're close to the target (<= 3 nodes missing), stop and reload weapons if needed

		  This is an advanced check, so simply ignore it if we are "noobs"
		*/
		if (canShoot && iAiGameType == GAM_RIFLES && iAiDiffLevel >=2)  {
			int num_reloaded=0;
			int i;
			for (i=0;i<5;i++) {
				if (!m_worm->tWeapons[i].Reloading)
					num_reloaded++;
			}

			if (num_reloaded <= 3)  {

				// If we see the target, fight instead of reloading!
				if (!we_see_the_target)  {
					NEW_ai_node_t *node = NEW_psLastNode;
					for(i=0; node && node != NEW_psCurrentNode; node = node->psPrev, i++) {}
					if (NEW_psLastNode == NULL || i>=3)  {
						// Reload weapons when we're far away from the target or if we don't have any path
						AI_ReloadWeapons();
					}
					if(NEW_psLastNode && (i>=3 && i<=5)) {
						// Stop, if we are not so far away
						if (fRopeAttachedTime >= 0.7f)
							m_worm->cNinjaRope.write().Clear();
						return;
					}
				}
			}
		}
	}

    /*
      Now that we've done all the boring stuff, our single job here is to reach the node.
      We have walking, jumping, move-through-air, and a ninja rope to help us.
    */

	{
		/*
		  If there is dirt between us and the next node, don't shoot a ninja rope
		  Instead, carve
		*/

		float traceDist = -1; int type;
		int length = m_worm->traceLine(nodePos, &traceDist, &type); // HINT: this is only a line, not the whole worm
															 // NOTE: this can return dirt, even if there's also rock between us two
		bool direct_traceLine_possible = (float)(length*length) > (nodePos-m_worm->vPos).GetLength2();

		// If the node is right above us, use a carving weapon
		if ((fabs(nodePos.x - m_worm->vPos.get().x) <= 50) && (type & PX_DIRT))
			if (nodePos.y < m_worm->vPos.get().y)  {
				int wpn;
				if(canShoot && (wpn = AI_FindClearingWeapon()) != -1) {
					m_worm->iCurrentWeapon = wpn;
					ws->bShoot = AI_SetAim(nodePos); // aim at the dirt and fire if aimed
					if(ws->bShoot) {
						// Don't do any crazy things when shooting
						ws->bMove = false;
						ws->bJump = false;
					}
				} else  {
					AI_SetAim(nodePos);
					AI_Jump();
					fireNinja = true; // we have no other option
				}
				/* else
					AI_SimpleMove(game.gameMap(),psAITarget != NULL); */ // no weapon found, so move around
			}

		// HINT: atm, fireNinja is always false here
		// if there is dirt in the way (and close to us), then don't use ninja rope
		if(!fireNinja && !direct_traceLine_possible && (type & PX_DIRT) && length < 10) {
			// HINT: as we always carve, we don't need to do it here specially

			// release rope, if it is atached and above
//			if(fRopeAttachedTime > 0.5f && cNinjaRope.getHookPos().y - 5.0f > nodePos.y)
//				cNinjaRope.Release();

			// Jump, if the node is above us
			if (nodePos.y < m_worm->vPos.get().y && m_worm->vPos.get().y - nodePos.y >= 10 && m_worm->vPos.get().y - nodePos.y <= 30)
				AI_Jump();

//			ws->bMove = true;

		} else  { // no dirt or something close, just some free way infront of us
			// use ninja rope in general, it's faster
			fireNinja = true;

			// If there's no dirt around and we have jetpack in our arsenal, lets use it!
			for (size_t i=0;i<m_worm->tWeapons.size();i++) {
				if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Recoil < 0 && !m_worm->tWeapons[i].Reloading)  {
					m_worm->iCurrentWeapon = i;
					ws->bShoot = AI_SetAim(nodePos);
					if(ws->bShoot) fireNinja = false;
					break;
				}
			}
		}
	}

	//
	//	Shooting the rope
	//

    // If the node is above us by a lot, we should use the ninja rope
	// If the node is far, jump and use the rope, too
	// TODO: the above comment doesn't match the code. why?
/*	if(fireNinja) {
		fireNinja = (NEW_psCurrentNode->fY+20 < vPos.y);
		if (!fireNinja && (fabs(NEW_psCurrentNode->fX-vPos.x) >= 50))  {
			// On ground? Jump
			AI_Jump();
		}
	}
*/

	// If we're above the node and the rope is hooked wrong, release the rope
/*	if(bOnGround && (vPos.y < nodePos.y)) {
		cNinjaRope.Release();
		fireNinja = false;
	}
*/

	// It has no sense to shoot the rope on short distances
	// TODO: it make sense, only not for realy short distances; i try it first now completly without this check
	// also, we shouldn't relay on ropespot here but better on nodePos; GetBestRopeSpot should get a good ropespot if nodePos is good
//	if(fireNinja && (vPos-ropespot).GetLength2() < 625.0f)
//		fireNinja = false;

	// In rifle games: don't continue if we see the final target and are quite close to it
	// If we shot the rope, we wouldnt aim the target, which is the priority now
	if(canShoot && fireNinja && iAiGameType == GAM_RIFLES && we_see_the_target && (m_worm->vPos-cPosTarget).GetLength2() <= 3600.0f) {
		fireNinja = false;
	}

	bool stillAimingRopeSpot = false;
	CVec ropespot;
    if(fireNinja) {
    	// set it to false, only if we pass the following checks, set it to true again
		fireNinja = false;

		// there could be multiple good ropespot and if we already aim at one then we should use this
		// AI_GetBestRopeSpot takes care therefore also of fAngle
		ropespot = AI_GetBestRopeSpot(nodePos);

		// Aim
		bool aim = AI_SetAim(ropespot);
		if(!aim) stillAimingRopeSpot = true;
		
        /*
          Got aim, so shoot a ninja rope
          also shoot, if we are not on ground
          We shoot a ninja rope if it isn't shot
          Or if it is, we make sure it has pulled us up and that it is attached
        */
        if(aim || !m_worm->bOnGround) {
			if(!m_worm->cNinjaRope.get().isReleased())
            	fireNinja = true;
            else {
				if(m_worm->cNinjaRope.get().isAttached()) {
					//float restlen_sq = cNinjaRope.getRestLength(); restlen_sq *= restlen_sq;
                    //if((vPos-cNinjaRope.getHookPos()).GetLength2() < restlen_sq && vVelocity.y<-10)
                    if(fRopeAttachedTime > 0.5f)
                    	fireNinja = true;
                }
            }
			if( (m_worm->vPos.get().y - NEW_psCurrentNode->fY) > 10.0f)  {
				AI_Jump();
			}
        }
    }

    if(fireNinja) {
		const CVec dir = m_worm->getFaceDirection();

    	// the final shoot of the rope...
		m_worm->cNinjaRope.write().Shoot(dir);
		fRopeHookFallingTime = 0;
		fRopeAttachedTime = 0;

    } else { // not fireNinja

		// Aim at the node
		if(!we_see_the_target && !stillAimingRopeSpot) AI_SetAim(nodePos);

		if(m_worm->canAirJump()) {
			if((m_worm->vPos.get().y > NEW_psCurrentNode->fY + 10) && fabs(m_worm->vPos.get().y - NEW_psCurrentNode->fY) > fabs(m_worm->vPos.get().x - NEW_psCurrentNode->fX))
				AI_Jump();
			ws->bMove = true;
		}
		// If the node is above us by a little, jump
		else if((m_worm->vPos.get().y-NEW_psCurrentNode->fY) <= 30 && (m_worm->vPos.get().y - NEW_psCurrentNode->fY) > 10) {
			if (!AI_Jump()) {
				ws->bMove = true; // if we should not jump, move
			}
		}
		
		if(stillAimingRopeSpot && isMoveGivingDisadvantage(m_worm->getPos() + (ropespot - m_worm->getPos()) * 0.8f, m_worm)) {
			ws->bMove = false;
			return;
		}
	}

	// If we are using the rope to fly up, it can happen, that we will fly through the node and continue in a wrong direction
	// To avoid this we check the velocity and if it is too high, we release the rope
	// When on ground rope does not make much sense mainly, but there are rare cases where it should stay like it is
	if (m_worm->cNinjaRope.get().isAttached() && !m_worm->bOnGround && (m_worm->cNinjaRope.get().getHookPos().y > m_worm->vPos.get().y) && (NEW_psCurrentNode->fY < m_worm->vPos.get().y)
		&& fabs(m_worm->cNinjaRope.get().getHookPos().x - m_worm->vPos.get().x) <= 50 && m_worm->vVelocity.get().y <= 0)  {
		CVec force;

		// Air drag (Mainly to dampen the ninja rope)
		// float Drag = cGameScript->getWorm()->AirFriction; // TODO: not used

		float dist = (CVec(NEW_psCurrentNode->fX, NEW_psCurrentNode->fY) - m_worm->vPos).GetLength();
		float time = sqrt(2*dist/(force.GetLength()));
		//float time2 = dist/vVelocity.GetLength();*/
		float diff = m_worm->vVelocity.get().y - ((float)cClient->getGameLobby()[FT_WormGravity] * time);
		if (diff < 0)
			m_worm->cNinjaRope.write().Clear();
	}


    // If we are stuck in the same position for a while, take measures to get out of being stuck
	if(fabs(cStuckPos.x - m_worm->vPos.get().x) < 5 && fabs(cStuckPos.y - m_worm->vPos.get().y) < 5) {
        fStuckTime += tLX->fDeltaTime;

        // Have we been stuck for a few seconds?
        if(fStuckTime.seconds() > 3) {
            // Jump, move, carve, switch directions and release the ninja rope
			AI_Jump();
            ws->bMove = true;
			ws->bCarve = true;

/*			AI_Carve(); */

            bStuck = true;
            fStuckPause = tLX->currentTime;

            m_worm->fAngle -= BotAngleSpeed * tLX->fDeltaTime.seconds();
            // Clamp the angle
			m_worm->fAngle = MIN(cClient->getGameLobby()[FT_FullAimAngle] ? 90.0f : 60.0f, m_worm->fAngle.get());
			m_worm->fAngle = MAX(-90.f, m_worm->fAngle.get());

			// Stucked too long?
			if (fStuckTime >= 5.0f)  {
				// Try the previous node
				if (NEW_psCurrentNode->psPrev)
					NEW_psCurrentNode = NEW_psCurrentNode->psPrev;
				fStuckTime = 0;
			}

            // Recalculate the path
            // TODO: should we do this in a more general way somewhere other?
            AI_CreatePath();

            return;
        }
    }
    else {
        bStuck = false;
        fStuckTime = 0;
        cStuckPos = m_worm->vPos;

    }

	if(canShoot)
		// only move if we are away from the next node
		ws->bMove = fabs(m_worm->vPos.get().x - NEW_psCurrentNode->fX) > 3.0f;
	else
		// always move, we cannot do something else
		ws->bMove = true;

/*
	// If the next node is above us by a little, jump too
	NEW_ai_node_t *nextNode = NEW_psCurrentNode->psNext;
	if (nextNode)  {
		if ((vPos.y-nextNode->fY) <= 30 && (vPos.y-nextNode->fY) > 0)
			if (!AI_Jump())
				ws->bMove = true; // if we should not jump, move
	}
*/



/*
    // If we're above the node, let go of the rope and move towards to node
    if(NEW_psCurrentNode->fY >= vPos.y) {
        // Let go of any rope
        cNinjaRope.Release();

        // Walk in the direction of the node
        ws->bMove = true;
    }
*/

}

void drawpoint(SDL_Surface * debug_surf, CVec point)
{
	DrawRectFill(debug_surf, (int)point.x * 2, (int)point.y * 2, (int)point.x * 2 + 4, (int)point.y * 2 + 4, Color(0, 255, 0));
}

///////////////////////
// Returns coordinates of a point that is best for shooting the target
CVec CWormBotInputHandler::AI_FindShootingSpot()
{
	CWorm* psAITarget = game.wormById(nAITargetWormId, false);
	if (psAITarget == NULL)
		return CVec(0,0);

	// If the worm is not on ground, we cannot hit him with a napalm-like weapon (napalm, blaster etc.)
	bool have_straight = false;
	bool have_falling = false;

	// Check what weapons we have
	for (size_t i=0; i < m_worm->tWeapons.size(); i++)  {
		if (m_worm->tWeapons[i].weapon() && m_worm->tWeapons[i].weapon()->Proj.Proj)  {
			// Get the gravity
			float gravity = 100.0f;  // Default
			if (m_worm->tWeapons[i].weapon()->Proj.Proj->UseCustomGravity)
				gravity = (float)m_worm->tWeapons[i].weapon()->Proj.Proj->Gravity;
			gravity *= (float)cClient->getGameLobby()[FT_ProjGravityFactor];
			
			// Change the flags according to the gravity
			if (gravity >= 5)
				have_falling = true;
			else if (gravity >= -5)
				have_straight = true;
		}
	}

	float upper_bound = 0;
	float lower_bound = 0;

	// Falling projectiles, we have to get above a bit
	if (have_falling || (have_straight && psAITarget->CheckOnGround()))  {
		lower_bound = (float)PI/2;
		upper_bound = 3 * (float)PI/2;

	// Flying straight
	} else if (have_straight)  {
		lower_bound = 0;
		upper_bound = 2 * (float)PI;

	// Flying up
	} else {
		lower_bound = (float)-PI/2;
		upper_bound = (float)PI/2;
	}


	// Check a best-distance upper half circle around the target
	CVec possible_pos;
	for (float j=lower_bound; j <= upper_bound; j += 0.3f)  {
		possible_pos.x = 40.0f * sinf(j) + psAITarget->getPos().x;
		possible_pos.y = 40.0f * cosf(j) + psAITarget->getPos().y;
		//PutPixel(game.gameMap()->GetDebugImage(), possible_pos.x * 2, possible_pos.y * 2, Color(255, 0, 0));

		if (AI_IsInAir(possible_pos, 1) && m_worm->traceLine(possible_pos, psAITarget->getPos(), NULL) >= 40)  {
			//drawpoint(game.gameMap()->GetDebugImage(), possible_pos);
			return possible_pos;
		}
	}

	// Let's try a bit worse pos - in the level of the target, but a bit distant from it
	possible_pos = psAITarget->getPos();
	for (int i = 0; i < 10; i++)  {
		possible_pos.x += 5;
		if (!traceWormLine(possible_pos, psAITarget->getPos(), NULL))  {
			possible_pos.x -= 5;
			if (fabs(psAITarget->getPos().x - possible_pos.x) >= 15)
				return possible_pos;
		}
	}

	possible_pos = psAITarget->getPos();
	for (int i = 0; i < 10; i++)  {
		possible_pos.x -= 5;
		if (!traceWormLine(possible_pos, psAITarget->getPos(), NULL))  {
			possible_pos.x -= 5;
			if (fabs(psAITarget->getPos().x - possible_pos.x) >= 15)
				return possible_pos;
		}
	}

	// Not found
	return possible_pos;
}

NEW_ai_node_t* get_last_ai_node(NEW_ai_node_t* n) {
	if(!n) return NULL;
	for(;n->psNext;n=n->psNext) {}
	return n;
}

void delete_ai_nodes(NEW_ai_node_t* start) {
	if(!start) return;
	delete_ai_nodes(start->psNext);
	delete start;
}

void delete_ai_nodes(NEW_ai_node_t* start, NEW_ai_node_t* end) {
	if(!start || start == end) return;
	delete_ai_nodes(start->psNext, end);
	delete start;
}

float get_ai_nodes_length(NEW_ai_node_t* start) {
	float l,dx,dy;
	for(l=0;start;start=start->psNext) {
		if(start->psNext) {
			dx = start->fX - start->psNext->fX;
			dy = start->fY - start->psNext->fY;
			l += sqrt(dx*dx + dy*dy);
		} else
			break;
	}
	return l;
}

float get_ai_nodes_length2(NEW_ai_node_t* start) {
	float l,dx,dy;
	for(l=0;start;start=start->psNext) {
		if(start->psNext) {
			dx = start->fX - start->psNext->fX;
			dy = start->fY - start->psNext->fY;
			l += dx*dx + dy*dy;
		} else
			break;
	}
	return l;
}



struct BotWormType : WormType {
	CWormInputHandler* createInputHandler(CWorm* w) { return new CWormBotInputHandler(w); }
	int toInt() { return 1; }
} PRF_COMPUTER_instance;
WormType* PRF_COMPUTER = &PRF_COMPUTER_instance;

CWormBotInputHandler::CWormBotInputHandler(CWorm* w) : CWormInputHandler(w) {
	nAIState = AI_THINK;
    //fLastWeaponSwitch = AbsTime();
	NEW_psPath = NULL;
	NEW_psCurrentNode = NULL;
	NEW_psLastNode = NULL;
	pathSearcher = NULL;	
	fLastFace = 0;
	fBadAimTime = 0;
	nAITargetWormId = -1;
	fLastShoot = 0; // for AI
	fLastJump = AbsTime();
	fLastWeaponChange = 0;
	fLastCompleting = AbsTime();
	fLastGoBack = AbsTime();

	if(w->tProfile.get())
		iAiDiffLevel = CLAMP(w->tProfile->nDifficulty, 0, 3);
	else
		iAiDiffLevel = AI_MEDIUM;
	
	AI_Initialize();
	
	game.onNewPlayer(this);
	game.onNewPlayer_Lua(this);
}

CWormBotInputHandler::~CWormBotInputHandler() {
	// Make sure the pathfinding ends
	AI_Shutdown();
}

void CWormBotInputHandler::onRespawn() {
	nAIState = AI_THINK;
	fLastShoot = 0;
	fLastGoBack = AbsTime();

	AI_Respawn();
}


void CWormBotInputHandler::initWeaponSelection() {

	if(game.gameScript()->gusEngineUsed())
		// TODO: for now where we dont have wpns merged (yet)
		// Gusanos will mostly (depending on mod) init with random wpns anyway
		return;
		
	// If this is an AI worm, lets give him a preset or random arsenal (but only with client side weapon selection)
		
	// TODO: move this to CWorm_AI
	bool bRandomWeaps = true;
	// Combo (rifle)
	if (((int)cClient->getGameLobby()[FT_LoadingTime] > 15 && (int)cClient->getGameLobby()[FT_LoadingTime] < 26) && 
		(cClient->getGameLobby()[FT_Mod].as<ModInfo>()->name.get().find("Classic") != std::string::npos ||
		 cClient->getGameLobby()[FT_Mod].as<ModInfo>()->name.get().find("Liero v1.0") != std::string::npos ))  {
		if (game.weaponRestrictions()->isEnabled("Rifle"))  {
			for (size_t i=0; i<m_worm->tWeapons.size(); i++)
				m_worm->weaponSlots.write()[i].WeaponId = game.gameScript()->FindWeaponId("Rifle");  // set all weapons to Rifle
			bRandomWeaps = false;
			AI_SetGameType(GAM_RIFLES);
		}
	}
	// 100 lt
	else if ((cClient->getGameLobby()[FT_Mod].as<ModInfo>()->name.get().find("Liero") != std::string::npos ||
			  cClient->getGameLobby()[FT_Mod].as<ModInfo>()->name.get().find("Classic") != std::string::npos) &&
			 (int)cClient->getGameLobby()[FT_LoadingTime] == 100)  {
		int MyWeaps = game.weaponRestrictions()->isEnabled("Super Shotgun") + game.weaponRestrictions()->isEnabled("Napalm") +  game.weaponRestrictions()->isEnabled("Cannon") + game.weaponRestrictions()->isEnabled("Doomsday") + game.weaponRestrictions()->isEnabled("Chaingun");
		if (MyWeaps == 5 && m_worm->tWeapons.size() >= 5)  {
			// Set our weapons
			m_worm->weaponSlots.write()[0].WeaponId = game.gameScript()->FindWeaponId("Super Shotgun");
			m_worm->weaponSlots.write()[1].WeaponId = game.gameScript()->FindWeaponId("Napalm");
			m_worm->weaponSlots.write()[2].WeaponId = game.gameScript()->FindWeaponId("Cannon");
			m_worm->weaponSlots.write()[3].WeaponId = game.gameScript()->FindWeaponId("Doomsday");
			m_worm->weaponSlots.write()[4].WeaponId = game.gameScript()->FindWeaponId("Chaingun");
			bRandomWeaps = false;
			AI_SetGameType(GAM_100LT);
		}
	}
	// Mortar game
	else if ((cClient->getGameLobby()[FT_Mod].as<ModInfo>()->name.get().find("MW 1.0") != std::string::npos ||
			  cClient->getGameLobby()[FT_Mod].as<ModInfo>()->name.get().find("Modern Warfare1.0") != std::string::npos) &&
			 (int)cClient->getGameLobby()[FT_LoadingTime] < 50)  {
		if (game.weaponRestrictions()->isEnabled("Mortar Launcher"))  {
			for (size_t i=0; i<m_worm->tWeapons.size(); i++)
				m_worm->weaponSlots.write()[i].WeaponId = game.gameScript()->FindWeaponId("Mortar Launcher");  // set all weapons to Mortar
			bRandomWeaps = false;
			AI_SetGameType(GAM_MORTARS);
		}
	}
	
	// Random
	if (bRandomWeaps) {
		m_worm->GetRandomWeapons();
		AI_SetGameType(GAM_OTHER);
	}

	for(size_t i = 0; i < m_worm->tWeapons.size(); ++i) {
		if(m_worm->tWeapons[i].weapon() == NULL)
			m_worm->weaponSlots.write()[i].WeaponId = game.getRandomEnabledWpn();
	}
	
	m_worm->bWeaponsReady = true;

}

void CWormBotInputHandler::doWeaponSelectionFrame(SDL_Surface * bmpDest, CViewport *v) {
	// Do nothing here. We can get here when we are waiting for the host worm to select his weapons.
	// In other cases, we would have already selected the weapons for the worm in initWeaponSelection().
	
	//bWeaponsReady = true;
	//iCurrentWeapon = 0;
}

void CWorm::setAiDiff(int aiDiff) {
	if(!m_inputHandler) {
		errors << "CWorm::setAiDiff: can only set ai-diff for local bots" << endl;
		return;
	}
	
	CWormBotInputHandler* ai = dynamic_cast<CWormBotInputHandler*> (m_inputHandler);
	if(!ai) {
		errors << "CWorm::setAiDiff: worm " << getID() << ":" << getName() << " is not a bot" << endl;
		return;
	}
	
	ai->setAiDiff(aiDiff);
}

void CWormBotInputHandler::setAiDiff(int aiDiff) {
	if(aiDiff < AI_EASY || aiDiff > AI_XTREME) {
		errors << "CWormBotInputHandler::setAiDiff: " << aiDiff << " is invalid" << endl;
		return;
	}
	
	iAiDiffLevel = aiDiff;
}


void CWormBotInputHandler::subThink() {
	if(!m_worm) return;
	
	if ( !m_worm->isActive() )
		baseActionStart(RESPAWN);

	// stupid wpn change code from player_ai.cpp
	if ( ( m_worm->getCurrentWeaponRef() && m_worm->getCurrentWeaponRef()->reloading && ( rand() % 8 == 0 ) ) || rand() % 15 == 0)
	{
		m_worm->changeWeaponTo(m_worm->getWeaponIndexOffset( rand() % 50 ) );
	}

}
