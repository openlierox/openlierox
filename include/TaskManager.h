/*
 *  TaskManager.h
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 09.02.09.
 *  code under LGPL
 *
 */

#ifndef __TASKMANAGER_H__
#define __TASKMANAGER_H__

#include <SDL_thread.h>
#include <string>
#include <set>
#include <list>
#include <typeinfo>
#include <boost/shared_ptr.hpp>
#include "ThreadPool.h"
#include "Mutex.h"
#include "RefCounter.h"
#include "util/Result.h"

class TaskManager;
struct CmdLineIntf;
struct SDL_Surface;

struct Task : Action {
	Task() : manager(NULL), state(TS_INVALID), mutex(new Mutex()), breakSignal(false), replacingTask(NULL) {}
	virtual ~Task() {}
	
	TaskManager* manager;
	std::string name;
	enum State { TS_QUEUED, TS_WAITFORIMMSTART, TS_RUNNING, TS_RUNNINGQUEUED, TS_INVALID } state;
	boost::shared_ptr<Mutex> mutex;
	volatile bool breakSignal;
	Task* replacingTask;

	virtual Result handle() = 0;
	virtual std::string statusText() { return ""; } // if this is not empty, OLX will show the status of this tasks if you are in the menu
};

struct LoopTask : Task {
	virtual Result handleFrame() = 0;
	virtual Result handle() {
		while(true) {
			if(breakSignal) return "break";
			if(NegResult r = handleFrame())
				return r.res;
		}
		return true;
	}
};

// small Task holder helper - task mutex is locked while this exists
struct ScopedTask : RefCounter {
	Task* task;
	ScopedTask() : task(NULL) {}
	ScopedTask(Task* t, bool doLock) : task(t) { if(task && doLock) task->mutex->lock(); }
	ScopedTask(const ScopedTask& t) : RefCounter(t), task(t.task) {}
	~ScopedTask() { RefCounter::uninit(); }
	void onLastRefRemoved() { if(task) task->mutex->unlock(); }
	operator bool() { return task != NULL; }
};

/*
 The TaskManager runs tasks. When running unqueued, it is very similar to what the ThreadPool does.
 
 The differences are:
 - Tasks are not seen as important. If the game quits for example, all not yet started tasks are just ignored.
 - There are multiple ways of queuing tasks. Including the possibility of breaking old tasks of the same type
   when they are obsolete by the new one.
 - Tasks can be shown to the user if you overload the statusText() function.
 - Tasks have a breakSignal. It is up to the implementer if you check this. However, for long tasks, you should
   check it, otherwise the task breaking would not really work. If you don't, nothing breaks though.
 */
class TaskManager {
private:
	SDL_mutex* mutex;
	SDL_cond* taskFinished;
	SDL_cond* queueThreadWakeup;
	bool quitSignal;
	std::set<Task*> runningTasks;
	std::list<Action*> queuedTasks;
	SmartPointer<ThreadPoolItem> queueThread;

	ScopedTask haveTaskOfType__unsafe(const std::type_info& taskType);
public:
	TaskManager();
	~TaskManager();
	
	enum QueueType { QT_NoQueue, QT_GlobalQueue, QT_QueueToSameTypeAndBreakCurrent };
	void start(Task* t, QueueType queue = QT_NoQueue);
	ScopedTask haveTaskOfType(const std::type_info& taskType); // call with typeid(TaskClass)
	void finishQueuedTasks();
	void dumpState(CmdLineIntf& cli) const;
	
	// Prints all tasks with a status text on the surface.
	// We do this in the menu. This function is implemented in MenuSystem.cpp.
	void renderTasksStatus(SDL_Surface* s);
};

extern TaskManager* taskManager;

void InitTaskManager();
void UnInitTaskManager();

#endif
