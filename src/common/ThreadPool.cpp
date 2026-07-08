/*
 *  ThreadPool.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 08.02.09.
 *  code under LGPL
 *
 */

#include <SDL_thread.h>
#include "ThreadPool.h"
#include "Debug.h"
#include "AuxLib.h"
#include "ReadWriteLock.h" // for ScopedLock
#include "OLXCommand.h"
#include "util/macros.h"


static bool isThreadIdValid(ThreadId id) {
	if(id == 0) return false;
	if(id == (ThreadId)-1) return false;
	return true;
}

ThreadId mainThreadId = -1;

bool isMainThread() {
	return mainThreadId == getCurrentThreadId();
}

ThreadId gameloopThreadId = -1;

bool isGameloopThread() {
	if(gameloopThreadId == (ThreadId)-1) {
		errors << "isGameloopThread: Gameloop thread is not running" << endl;
		return false;
	}
	return gameloopThreadId == getCurrentThreadId();
}

bool isGameloopThreadRunning() {
	return gameloopThreadId != (ThreadId)-1;
}

void getAllThreads(std::set<ThreadId>& ids) {
	if(isThreadIdValid(mainThreadId))
		ids.insert(mainThreadId);
	if(isThreadIdValid(gameloopThreadId))
		ids.insert(gameloopThreadId);

	if(threadPool) {
		std::map<ThreadId, std::string> threads;
		threadPool->getAllWorkingThreads(threads);
		foreach(t, threads) {
			ids.insert(t->first);
		}
	}
}


// The reused worker thread.
// The task it currently runs is a separate, reference-counted ThreadPoolItem handle,
// so the caller never sees or holds the worker
// and there is no lifetime coupling between the two.
struct ThreadWorker {
	ThreadPool* pool;
	SDL_Thread* thread;
	ThreadId nativeThreadId;
	SmartPointer<ThreadPoolItem> task; // current task, or null when idle
	Action* action;                    // current action, or null when idle
	ThreadWorker() : pool(NULL), thread(NULL), nativeThreadId(0), action(NULL) {}
};


ThreadPool::ThreadPool(unsigned int size) {
	nextAction = NULL;
	quitting = false;
	aliveWorkers = 0;
	mutex = SDL_CreateMutex();
	awakeThread = SDL_CreateCond();
	threadStartedWork = SDL_CreateCond();
	threadStatusChanged = SDL_CreateCond();
	taskFinished = SDL_CreateCond();
	startMutex = SDL_CreateMutex();

	notes << "ThreadPool: creating " << size << " threads ..." << endl;
	while(availableThreads.size() < size)
		prepareNewThread();
}

ThreadPool::~ThreadPool() {
	waitAll();

	// All workers are idle now.
	// Tell them to quit,
	// and wait until every worker has actually returned from threadWrapper
	// before we free anything.
	// We wait on aliveWorkers rather than relying on SDL_WaitThread to block,
	// because a worker still parked in SDL_CondWait
	// must not be left touching the mutex/conds once we destroy them.
	SDL_mutexP(mutex);
	nextAction = NULL;
	quitting = true;
	SDL_CondBroadcast(awakeThread);
	while(aliveWorkers > 0)
		SDL_CondWait(threadStatusChanged, mutex);
	SDL_mutexV(mutex);

	// The workers are done; reclaim their thread handles and free them.
	for(std::set<ThreadWorker*>::iterator i = availableThreads.begin(); i != availableThreads.end(); ++i) {
		SDL_WaitThread((*i)->thread, NULL);
		delete *i;
	}
	availableThreads.clear();

	SDL_DestroyMutex(startMutex);
	SDL_DestroyCond(taskFinished);
	SDL_DestroyCond(threadStartedWork);
	SDL_DestroyCond(threadStatusChanged);
	SDL_DestroyCond(awakeThread);
	SDL_DestroyMutex(mutex);
}

void ThreadPool::prepareNewThread() {
	ThreadWorker* w = new ThreadWorker();
	w->pool = this;
	availableThreads.insert(w);
	w->thread = SDL_CreateThread(threadWrapper, "ThreadPool worker", w);
}

int ThreadPool::threadWrapper(void* param) {
	ThreadWorker* w = (ThreadWorker*)param;
	w->nativeThreadId = getCurrentThreadId();
	ThreadPool* pool = w->pool;

	SDL_mutexP(pool->mutex);
	pool->aliveWorkers++;
	while(true) {
		while(pool->nextAction == NULL && !pool->quitting)
			SDL_CondWait(pool->awakeThread, pool->mutex);
		if(pool->quitting) break;

		// Take the pending task.
		w->action = pool->nextAction; pool->nextAction = NULL;
		w->task = pool->nextTask; pool->nextTask = NULL;
		pool->availableThreads.erase(w);
		pool->usedThreads.insert(w);
		SDL_CondSignal(pool->threadStartedWork); // let start() know we took it
		SDL_mutexV(pool->mutex);

		setCurThreadName(w->task->name);
		int ret = w->action->handle();
		delete w->action; w->action = NULL;
		setCurThreadName(w->task->name + " [finished]");

		SDL_mutexP(pool->mutex);
		w->task->ret = ret;
		w->task->finished = true;
		w->task = NULL; // drop the worker's reference; the handle keeps the result
		// The worker goes idle immediately: the result lives in the handle,
		// so there is nothing to wait for a caller to "collect".
		pool->usedThreads.erase(w);
		pool->availableThreads.insert(w);
		SDL_CondBroadcast(pool->taskFinished);     // wake any wait() on this task
		SDL_CondSignal(pool->threadStatusChanged); // wake waitAll()
		setCurThreadName("");
	}

	// Woken by ~ThreadPool: mark ourselves gone before releasing the mutex,
	// so that once aliveWorkers hits 0 no worker can still touch the pool.
	pool->aliveWorkers--;
	SDL_CondSignal(pool->threadStatusChanged);
	SDL_mutexV(pool->mutex);
	return 0;
}

SmartPointer<ThreadPoolItem> ThreadPool::start(Action* act, const std::string& name) {
	SDL_mutexP(startMutex); // serialize start() so nextAction/nextTask are not clobbered
	SDL_mutexP(mutex);
	if(availableThreads.size() == 0) {
#ifndef SINGLETHREADED
		warnings << "no available thread in ThreadPool for " << name << ", creating new one..." << endl;
#endif
		prepareNewThread();
	}
	assert(nextAction == NULL);
	SmartPointer<ThreadPoolItem> task = new ThreadPoolItem();
	task->name = name;
	nextAction = act;
	nextTask = task;

	SDL_CondSignal(awakeThread);
	while(nextAction != NULL) SDL_CondWait(threadStartedWork, mutex); // wait until a worker took it
	nextTask = NULL;
	SDL_mutexV(mutex);

	SDL_mutexV(startMutex);
	return task;
}

SmartPointer<ThreadPoolItem> ThreadPool::start(ThreadFunc fct, void* param, const std::string& name) {
	struct StaticAction : Action {
		ThreadFunc fct; void* param;
		Result handle() { return (*fct) (param); }
	};
	StaticAction* act = new StaticAction();
	act->fct = fct;
	act->param = param;
	return start(act, name);
}

SmartPointer<ThreadPoolItem> ThreadPool::start(boost::function<Result()> fct, const std::string& name) {
	struct FctPtrAction : Action {
		boost::function<Result()> fct;
		Result handle() { return fct(); }
	};
	FctPtrAction* act = new FctPtrAction();
	act->fct = fct;
	return start(act, name);
}

bool ThreadPool::wait(const SmartPointer<ThreadPoolItem>& item, int* status) {
	if(!item.get()) return false;
	SDL_mutexP(mutex);
	while(!item->finished) SDL_CondWait(taskFinished, mutex);
	if(status) *status = item->ret;
	SDL_mutexV(mutex);
	return true;
}

bool ThreadPool::waitAll() {
	SDL_mutexP(mutex);
	while(usedThreads.size() > 0) {
		warnings << "ThreadPool: waiting for " << usedThreads.size() << " task(s) to finish:" << endl;
		for(std::set<ThreadWorker*>::iterator i = usedThreads.begin(); i != usedThreads.end(); ++i) {
			SmartPointer<ThreadPoolItem> t = (*i)->task;
			warnings << "  task " << (t.get() ? t->name : std::string("?")) << " is still working" << endl;
		}
		SDL_CondWait(threadStatusChanged, mutex);
	}
	SDL_mutexV(mutex);

	return true;
}

void ThreadPool::dumpState(CmdLineIntf& cli) const {
	ScopedLock lock(mutex);
	for(std::set<ThreadWorker*>::const_iterator i = usedThreads.begin(); i != usedThreads.end(); ++i) {
		SmartPointer<ThreadPoolItem> t = (*i)->task;
		cli.writeMsg("task '" + (t.get() ? t->name : std::string("?")) + "': working");
	}
}

void ThreadPool::getAllWorkingThreads(std::map<ThreadId, std::string>& threads) {
	ScopedLock lock(mutex);
	for(std::set<ThreadWorker*>::const_iterator i = usedThreads.begin(); i != usedThreads.end(); ++i) {
		SmartPointer<ThreadPoolItem> t = (*i)->task;
		if(t.get())
			threads[(*i)->nativeThreadId] = t->name;
	}
}


ThreadPool* threadPool = NULL;

void InitThreadPool(unsigned int size) {
#ifdef SINGLETHREADED
	size = 0;
#endif
	if(!threadPool)
		threadPool = new ThreadPool(size);
	else
		errors << "ThreadPool inited twice" << endl;
}

void UnInitThreadPool() {
	if(threadPool) {
		delete threadPool;
		threadPool = NULL;
	} else
		errors << "ThreadPool already uninited" << endl;
}
