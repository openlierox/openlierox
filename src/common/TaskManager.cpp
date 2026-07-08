/*
 *  TaskManager.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 09.02.09.
 *  code under LGPL
 *
 */

#include "ThreadPool.h"
#include "TaskManager.h"
#include "Debug.h"
#include "ReadWriteLock.h"
#include "OLXCommand.h"

TaskManager* taskManager = NULL;

void InitTaskManager() {
	if(!taskManager) {
		taskManager = new TaskManager();
	}
}

void UnInitTaskManager() {
	if(taskManager) {
		delete taskManager;
		taskManager = NULL;
	}
}


TaskManager::TaskManager() {
	quitSignal = false;
	mutex = SDL_CreateMutex();
	taskFinished = SDL_CreateCond();
	queueThreadWakeup = SDL_CreateCond();
		
	struct QueuedTaskHandler : Action {
		TaskManager* manager;
		QueuedTaskHandler(TaskManager* m) : manager(m) {}
		Result handle() {
			ScopedLock lock(manager->mutex);
			while(true) {
				if(manager->queuedTasks.size() > 0) {
					Action* act = manager->queuedTasks.front();
					manager->queuedTasks.pop_front();
					SDL_mutexV(manager->mutex);
					act->handle();
					delete act;
					SDL_mutexP(manager->mutex);
					continue;
				}
				if(manager->quitSignal) {
					return true;
				}
				SDL_CondWait(manager->queueThreadWakeup, manager->mutex);
			}
		}
	};
	queueThread = threadPool->start(new QueuedTaskHandler(this), "queued task handler");
}

TaskManager::~TaskManager() {
	finishQueuedTasks();

	SDL_mutexP(mutex);
	while(runningTasks.size() > 0) {
		notes << "waiting for " << runningTasks.size() << " task(s) to finish:" << endl;
		for(std::set<Task*>::iterator i = runningTasks.begin(); i != runningTasks.end(); ++i) {
			notes << "  " << (*i)->name << endl;
			(*i)->breakSignal = true;
		}
		SDL_CondWait(taskFinished, mutex);
	}
	SDL_mutexV(mutex);
		
	SDL_DestroyMutex(mutex);
	SDL_DestroyCond(taskFinished);
	SDL_DestroyCond(queueThreadWakeup);
}


void TaskManager::start(Task* t, QueueType queue) {
	Mutex::ScopedLock tlock(*t->mutex);
	ScopedLock lock(mutex);

	if(quitSignal) {
		warnings << "tried to start queued task " << t->name << " when queued task manager was already shutdown" << endl;
		return;
	}
	
	if(queue == QT_QueueToSameTypeAndBreakCurrent) {
		ScopedTask oldTask = haveTaskOfType__unsafe(typeid(*t));
		if(oldTask) {
			if(t->manager != NULL)
				errors << "Task->manager must be NULL" << endl;
			t->manager = this;

			oldTask.task->breakSignal = true;
						oldTask.task->replacingTask = t;
			return;
		}
		
		// we don't have such a task right now, so start a new one unqeued
		queue = QT_NoQueue;
	}
	
	runningTasks.insert(t);
	
	struct TaskHandler : Action {
		Task* task;
		Result handle() {
			if(task->manager == NULL) {
				errors << "TaskManager::TaskHandler: invalid task with unset manager" << endl;
				return "invalid task with unset manager";
			}
			{
				Mutex::ScopedLock lock(*task->mutex);
				task->state = Task::TS_QUEUED ? Task::TS_RUNNINGQUEUED : Task::TS_RUNNING;
			}
			Result ret = task->breakSignal ?
						Result("breaked even before start") :
						task->handle();
			{
				SDL_mutexP(task->manager->mutex);
				boost::shared_ptr<Mutex> m = task->mutex;
				Mutex::ScopedLock lock(*m);
				
								if(task->replacingTask) {
					if(!task->manager->quitSignal) {
												Mutex::ScopedLock lock(*task->replacingTask->mutex);
												task->manager->runningTasks.insert(task->replacingTask);
						TaskHandler* handler = new TaskHandler();
												handler->task = task->replacingTask;
												task->replacingTask->state = Task::TS_WAITFORIMMSTART;
												threadPool->start(handler, task->replacingTask->name + " handler");
					}
					else
						// We were requested to quit, i.e. we should not start this queued task anymore.
												delete task->replacingTask;
				}
				
				task->manager->runningTasks.erase(task);
				SDL_CondSignal(task->manager->taskFinished);
				SDL_mutexV(task->manager->mutex);
				delete task;
			}			
			return ret;
		}
	};
	TaskHandler* handler = new TaskHandler();
	handler->task = t;
	if(t->manager != NULL)
		errors << "Task->manager must be NULL" << endl;
	t->manager = this;
	
	if(queue == QT_NoQueue) {
		t->state = Task::TS_WAITFORIMMSTART;
		threadPool->start(handler, t->name + " handler");
	} else {
		t->state = Task::TS_QUEUED;
		queuedTasks.push_back(handler);
		SDL_CondSignal(queueThreadWakeup);
	}
}

ScopedTask TaskManager::haveTaskOfType__unsafe(const std::type_info& taskType) {
	for(std::set<Task*>::const_iterator i = runningTasks.begin(); i != runningTasks.end(); ++i) {
		if(typeid(**i) == taskType) {
			ScopedTask t(*i, true); // this will also do the lock on the tasks mutex
			return t;
		}
	}
	return ScopedTask();
}

ScopedTask TaskManager::haveTaskOfType(const std::type_info& taskType) {
	ScopedLock lock(mutex);
	ScopedTask t(haveTaskOfType__unsafe(taskType));
	return t;
}

void TaskManager::finishQueuedTasks() {
	{
		ScopedLock lock(mutex);
		for(std::set<Task*>::const_iterator i = runningTasks.begin(); i != runningTasks.end(); ++i)
			(*i)->breakSignal = true;
		if(quitSignal) // means that we have already finished
			return;
		quitSignal = true;
		SDL_CondSignal(queueThreadWakeup);
	}
	
	threadPool->wait(queueThread);
	queueThread = NULL;
}


void TaskManager::dumpState(CmdLineIntf& cli) const {
	ScopedLock lock(mutex);
	for(std::set<Task*>::const_iterator i = runningTasks.begin(); i != runningTasks.end(); ++i) {
		Task::State state;
		{
			Mutex::ScopedLock lock(*(*i)->mutex);
			state = (*i)->state;
		}
		switch(state) {
			case Task::TS_QUEUED: cli.writeMsg("task '" + (*i)->name + "': queued for execution"); break;
			case Task::TS_WAITFORIMMSTART: cli.writeMsg("task '" + (*i)->name + "': wait for immediate start"); break;
			case Task::TS_RUNNINGQUEUED: cli.writeMsg("task '" + (*i)->name + "': running from queue"); break;
			case Task::TS_RUNNING: cli.writeMsg("task '" + (*i)->name + "': running independently"); break;
			case Task::TS_INVALID: cli.writeMsg("task '" + (*i)->name + "': invalid state"); break;
		}
	}
	if(runningTasks.size() == 0)
		cli.writeMsg("no tasks queued/running");
}

