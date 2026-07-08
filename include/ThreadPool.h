/*
 *  ThreadPool.h
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 08.02.09.
 *  code under LGPL
 *
 */

#ifndef __OLX__THREADPOOL_H__
#define __OLX__THREADPOOL_H__

#include <stdint.h>
#include <set>
#include <map>
#include <string>
#include <mutex>
#include <condition_variable>
#include <boost/function.hpp>
#include "util/Result.h"
#include "ThreadId.h"
#include "SmartPointer.h"

class ThreadPool;
typedef Result (*ThreadFunc) (void*);
struct CmdLineIntf;

struct Action {
	virtual ~Action() {}
	virtual Result handle() = 0;
};

// The handle returned by ThreadPool::start().
// It represents one task (one start() call),
// not the reused worker thread that runs it.
// It is reference counted (held via SmartPointer),
// so it stays alive as long as the caller or the running worker holds it,
// and it can never be freed or reused under the caller.
// wait() waits on this, so it is always safe, even at shutdown.
struct ThreadPoolItem {
	std::string name;
	bool finished;
	int ret;
	ThreadPoolItem() : finished(false), ret(0) {}
};

// The reused worker thread, internal to the pool. Defined in ThreadPool.cpp.
struct ThreadWorker;

class ThreadPool {
private:
	mutable std::mutex mutex;
	std::condition_variable awakeThread;
	std::condition_variable threadStartedWork;
	std::condition_variable threadStatusChanged; // a worker became idle (for waitAll)
	std::condition_variable taskFinished;        // a task finished (for wait)
	Action* nextAction; std::string nextName;
	SmartPointer<ThreadPoolItem> nextTask;
	bool quitting;
	std::set<ThreadWorker*> availableThreads;
	std::set<ThreadWorker*> usedThreads;
	void prepareNewThread();
	static void threadWrapper(ThreadWorker* w);
	std::mutex startMutex;
public:
	ThreadPool(unsigned int size = 5);
	~ThreadPool();

	SmartPointer<ThreadPoolItem> start(ThreadFunc fct, void* param = NULL, const std::string& name = "unknown worker");
	SmartPointer<ThreadPoolItem> start(Action* act, const std::string& name = "unknown worker"); // ThreadPool will own and free the Action
	SmartPointer<ThreadPoolItem> start(boost::function<Result()> fct, const std::string& name = "unknown worker");
	bool wait(const SmartPointer<ThreadPoolItem>& item, int* status = NULL);
	bool waitAll();
	void dumpState(CmdLineIntf& cli) const;
	void getAllWorkingThreads(std::map<ThreadId, std::string>& threads);
};

extern ThreadPool* threadPool;

void InitThreadPool(unsigned int size = 40);
void UnInitThreadPool();

extern ThreadId mainThreadId;
bool isMainThread();
extern ThreadId gameloopThreadId;
bool isGameloopThread();
bool isGameloopThreadRunning();
ThreadId getCurrentThreadId();

void getAllThreads(std::set<ThreadId>& ids);
std::string getThreadName(ThreadId t); // Note: somewhat slow, use only for debugging
std::string getCurThreadName();

template<typename _T>
struct _ThreadFuncWrapper {
	typedef Result (_T::* FuncPointer)();
	template< FuncPointer _func >
		struct Wrapper {
			static Result wrapper(void* obj) {
				return (((_T*)obj) ->* _func)();
			}

			static SmartPointer<ThreadPoolItem> startThread(_T* const obj, const std::string& name) {
				return threadPool->start((ThreadFunc)&wrapper, (void*)obj, name);
			}
		};
};

#define StartMemberFuncInThread(T, memberfunc, name) \
_ThreadFuncWrapper<T>::Wrapper<&memberfunc>::startThread(this, name)


#endif
