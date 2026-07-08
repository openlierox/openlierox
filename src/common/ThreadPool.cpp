/*
 *  ThreadPool.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 08.02.09.
 *  code under LGPL
 *
 */

#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThreadPool.h"
#include "Debug.h"
#include "AuxLib.h"
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
	std::thread thread;
	ThreadId nativeThreadId;
	SmartPointer<ThreadPoolItem> task; // current task, or null when idle
	Action* action;                    // current action, or null when idle
	ThreadWorker() : pool(NULL), nativeThreadId(0), action(NULL) {}
};


ThreadPool::ThreadPool(unsigned int size) {
	nextAction = NULL;
	quitting = false;

	notes << "ThreadPool: creating " << size << " threads ..." << endl;
	while(availableThreads.size() < size)
		prepareNewThread();
}

ThreadPool::~ThreadPool() {
	waitAll();

	// All workers are idle now. Tell them to quit, then join each one.
	{
		std::lock_guard<std::mutex> lock(mutex);
		nextAction = NULL;
		quitting = true;
		awakeThread.notify_all();
	}

	// std::thread::join() is a real join: it returns only once the worker
	// has fully returned from threadWrapper, so no worker can still touch
	// the mutex/conds afterwards. No separate alive-count is needed
	// (unlike SDL_WaitThread, which could return before the thread finished).
	for(std::set<ThreadWorker*>::iterator i = availableThreads.begin(); i != availableThreads.end(); ++i) {
		(*i)->thread.join();
		delete *i;
	}
	availableThreads.clear();
}

void ThreadPool::prepareNewThread() {
	ThreadWorker* w = new ThreadWorker();
	w->pool = this;
	availableThreads.insert(w);
	w->thread = std::thread(threadWrapper, w);
}

void ThreadPool::threadWrapper(ThreadWorker* w) {
	setCurThreadName("ThreadPool worker");
	w->nativeThreadId = getCurrentThreadId();
	ThreadPool* pool = w->pool;

	std::unique_lock<std::mutex> lock(pool->mutex);
	while(true) {
		while(pool->nextAction == NULL && !pool->quitting)
			pool->awakeThread.wait(lock);
		if(pool->quitting) break;

		// Take the pending task.
		w->action = pool->nextAction; pool->nextAction = NULL;
		w->task = pool->nextTask; pool->nextTask = NULL;
		pool->availableThreads.erase(w);
		pool->usedThreads.insert(w);
		pool->threadStartedWork.notify_one(); // let start() know we took it
		lock.unlock();

		setCurThreadName(w->task->name);
		int ret = w->action->handle();
		delete w->action; w->action = NULL;
		setCurThreadName(w->task->name + " [finished]");

		lock.lock();
		w->task->ret = ret;
		w->task->finished = true;
		w->task = NULL; // drop the worker's reference; the handle keeps the result
		// The worker goes idle immediately: the result lives in the handle,
		// so there is nothing to wait for a caller to "collect".
		pool->usedThreads.erase(w);
		pool->availableThreads.insert(w);
		pool->taskFinished.notify_all();        // wake any wait() on this task
		pool->threadStatusChanged.notify_one(); // wake waitAll()
		setCurThreadName("");
	}

	// Woken by ~ThreadPool: just return. The destructor joins us after this.
}

SmartPointer<ThreadPoolItem> ThreadPool::start(Action* act, const std::string& name) {
	std::lock_guard<std::mutex> startLock(startMutex); // serialize start() so nextAction/nextTask are not clobbered
	std::unique_lock<std::mutex> lock(mutex);
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

	awakeThread.notify_one();
	while(nextAction != NULL) threadStartedWork.wait(lock); // wait until a worker took it
	nextTask = NULL;
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
	std::unique_lock<std::mutex> lock(mutex);
	while(!item->finished) taskFinished.wait(lock);
	if(status) *status = item->ret;
	return true;
}

bool ThreadPool::waitAll() {
	std::unique_lock<std::mutex> lock(mutex);
	while(usedThreads.size() > 0) {
		warnings << "ThreadPool: waiting for " << usedThreads.size() << " task(s) to finish:" << endl;
		for(std::set<ThreadWorker*>::iterator i = usedThreads.begin(); i != usedThreads.end(); ++i) {
			SmartPointer<ThreadPoolItem> t = (*i)->task;
			warnings << "  task " << (t.get() ? t->name : std::string("?")) << " is still working" << endl;
		}
		threadStatusChanged.wait(lock);
	}

	return true;
}

void ThreadPool::dumpState(CmdLineIntf& cli) const {
	std::lock_guard<std::mutex> lock(mutex);
	for(std::set<ThreadWorker*>::const_iterator i = usedThreads.begin(); i != usedThreads.end(); ++i) {
		SmartPointer<ThreadPoolItem> t = (*i)->task;
		cli.writeMsg("task '" + (t.get() ? t->name : std::string("?")) + "': working");
	}
}

void ThreadPool::getAllWorkingThreads(std::map<ThreadId, std::string>& threads) {
	std::lock_guard<std::mutex> lock(mutex);
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
