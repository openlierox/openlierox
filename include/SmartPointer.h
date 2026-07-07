/*
	OpenLieroX
	code under LGPL
	27-12-2007 Albert Zeyer
*/

#ifndef __SMARTPOINTER_H__
#define __SMARTPOINTER_H__

#include <limits.h>
#include <cassert>
#include <SDL_mutex.h>

#ifdef DEBUG
#include <map>
#include "Debug.h"
#endif

#include "Functors.h"

template < typename _Type, typename _SpecificInitFunctor >
class SmartPointer;

// Default de-initialization action is to call operator delete, for each object type.
template < typename _Type >
void SmartPointer_ObjectDeinit( _Type * obj )
{
	delete obj;
}

// these forward-declaration are needed here
struct SDL_Surface;
struct SDL_Texture;
struct SDL_Renderer;
struct SDL_Window;
class SoundSample;
class CMap;
class CGameScript;

// Specialized de-init functions, for each simple struct-like type that has no destructor
template <> void SmartPointer_ObjectDeinit<SDL_Surface> ( SDL_Surface * obj ); // Calls gfxFreeSurface(obj);
template <> void SmartPointer_ObjectDeinit<SDL_Texture> ( SDL_Texture * obj );
template <> void SmartPointer_ObjectDeinit<SDL_Renderer> ( SDL_Renderer * obj );
template <> void SmartPointer_ObjectDeinit<SDL_Window> ( SDL_Window * obj );
template <> void SmartPointer_ObjectDeinit<SoundSample> ( SoundSample * obj ); // Calls FreeSoundSample(obj);
template <> void SmartPointer_ObjectDeinit<CMap> ( CMap * obj ); // Requires to be defined elsewhere
template <> void SmartPointer_ObjectDeinit<CGameScript> ( CGameScript * obj ); // Requires to be defined elsewhere

#ifdef DEBUG
extern SDL_mutex *SmartPointer_CollMutex;
extern std::map< void *, SDL_mutex * > * SmartPointer_CollisionDetector;
#endif

/*
	standard smartpointer based on simple refcounting

	The refcounting is multithreading safe in this class,
	you can have copies of this object in different threads.
	Though it's not designed to operate with the same
	object in different threads. Also there is absolutly no
	thread safty on the pointer itself, you have to care
	about this yourself.
*/

/*template < typename _Obj >
class SmartObject;*/

template < typename _Type, typename _SpecificInitFunctor = NopFunctor<void*> >
class SmartPointer { //friend class SmartObject<_Type>;
public:
	typedef _Type value_type;
private:
	_Type* obj;
	int* refCount;
	SDL_mutex* mutex;


	void init(_Type* newObj) {
#ifdef DEBUG
		if (SmartPointer_CollMutex == NULL)
			SmartPointer_CollMutex = SDL_CreateMutex();
#endif

		if( newObj == NULL )
			return;
		if(!mutex) {
			mutex = SDL_CreateMutex();
			obj = newObj;
			refCount = new int;
			*refCount = 1;
			#ifdef DEBUG
			SDL_LockMutex(SmartPointer_CollMutex);
			if( SmartPointer_CollisionDetector == NULL )
			{
				//hints << "SmartPointer collision detector initialized" << endl; // Causes a crash during static init
				SmartPointer_CollisionDetector = new std::map< void *, SDL_mutex * > ();
			}
			if( SmartPointer_CollisionDetector->count(obj) != 0 ) // Should be faster than find() I think
			{
				errors << "ERROR! SmartPointer collision detected, old mutex " << (*SmartPointer_CollisionDetector)[obj] 
						<< ", new ptr (" << this << " " << obj << " " << refCount << " " << mutex << " " << (refCount?*refCount:-99) << ") new " << newObj << endl;
				SDL_UnlockMutex(SmartPointer_CollMutex);
				assert(false); // TODO: maybe do smth like *(int *)NULL = 1; to generate coredump? Simple assert(false) won't help us a lot
			}
			else
				SmartPointer_CollisionDetector->insert( std::make_pair( obj, mutex ) );
			SDL_UnlockMutex(SmartPointer_CollMutex);
			#endif
		}
	}

	void reset() {
		if(mutex) {
			lock();
			(*refCount)--;
			if(*refCount == 0) {
				#ifdef DEBUG
				SDL_LockMutex(SmartPointer_CollMutex);
				if( !SmartPointer_CollisionDetector || SmartPointer_CollisionDetector->count(obj) == 0 )
				{
					errors << "ERROR! SmartPointer already deleted reference ("
							<< this << " " << obj << " " << refCount << " " << mutex << " " << (refCount?*refCount:-99) << ")" << endl;
					if(!SmartPointer_CollisionDetector)
						errors << "SmartPointer_CollisionDetector is already uninitialised" << endl;
					SDL_UnlockMutex(SmartPointer_CollMutex);
					assert(false);
				}
				else
				{
					SmartPointer_CollisionDetector->erase(obj);
					if( SmartPointer_CollisionDetector->empty() )
					{
						delete SmartPointer_CollisionDetector;
						SmartPointer_CollisionDetector = NULL;
						if (SmartPointer_CollMutex)
						{
							SDL_DestroyMutex(SmartPointer_CollMutex);
							SmartPointer_CollMutex = NULL;
						}

						// WARNING: this is called at a very end for global objects and most other objects are already uninitialised.
						// For me, even the internal string structure doesn't work anymore (I get a std::length_error) and thus we cannot use the logging system.
						// TODO: Remove any global objects! We should not have any globals, at least not such complex globals.
						#ifdef DEBUG						
						printf("SmartPointer collision detector de-initialized, everything is freed now\n");
						#endif
					}
				}
				if( SmartPointer_CollMutex )
					SDL_UnlockMutex(SmartPointer_CollMutex);
				#endif
				SmartPointer_ObjectDeinit( obj );
				delete refCount; // safe, because there is no other ref anymore
				obj = NULL;
				refCount = NULL;
				unlock();
				SDL_DestroyMutex(mutex); // safe because there is no other ref anymore
				mutex = NULL;
		 	} else
		 		unlock();
		}
		obj = NULL;
		refCount = NULL;
		mutex = NULL;
	}

	void incCounter() {
		assert(*refCount > 0 && *refCount < INT_MAX);
		(*refCount)++;
	}

	void lock() {
		//printf("SmartPointer::lock    (%10p %10p %10p %10p %3i)\n", this, obj, refCount, mutex, refCount?*refCount:-99);
		SDL_mutexP(mutex);
	}
	void unlock() {
		//printf("SmartPointer::unlock  (%10p %10p %10p %10p %3i)\n", this, obj, refCount, mutex, refCount?*refCount:-99);
		SDL_mutexV(mutex);
	}

public:
	SmartPointer() : obj(NULL), refCount(NULL), mutex(NULL) {
		//printf("SmartPointer::construc(%10p %10p %10p %10p %3i)\n", this, obj, refCount, mutex, refCount?*refCount:-99);
		_SpecificInitFunctor()(this);
	}
	~SmartPointer() {
		//printf("SmartPointer::destruct(%10p %10p %10p %10p %3i)\n", this, obj, refCount, mutex, refCount?*refCount:-99);
		reset();
	}

	// Default copy constructor and operator=
	// If you specify any template<> params here these funcs will be silently ignored by compiler
	SmartPointer(const SmartPointer& pt) : obj(NULL), refCount(NULL), mutex(NULL) { operator=(pt); }
	SmartPointer& operator=(const SmartPointer& pt) {
		if(mutex == pt.mutex) return *this; // ignore this case
		reset();
		mutex = pt.mutex;
		if(mutex) {
			lock();
			obj = pt.obj; refCount = pt.refCount;
			incCounter();
			unlock();
		} else { obj = NULL; refCount = NULL; }
		return *this;
	}

	// WARNING: Be carefull, don't assing a pointer to different SmartPointer objects,
	// else they will get freed twice in the end. Always copy the SmartPointer itself.
	// In short: SmartPointer ptr(SomeObj); SmartPointer ptr1( ptr.get() ); // It's wrong, don't do that.
	SmartPointer(_Type* pt): obj(NULL), refCount(NULL), mutex(NULL) { operator=(pt); }
	SmartPointer& operator=(_Type* pt) {
		//printf("SmartPointer::op=Type (%10p %10p %10p %10p %3i)\n", this, obj, refCount, mutex, refCount?*refCount:-99);
		if(obj == pt) return *this; // ignore this case
		reset();
		init(pt);
		return *this;
	}

	_Type* get() const { return obj; }	// The smartpointer itself won't change when returning address of obj, so it's const.

	// HINT: no convenient cast functions in this class to avoid error-prone automatic casts
	// (which would lead to collisions!)
	// This operator is safe though.
	_Type * operator -> () const { return obj; };
	
	// refcount may be changed from another thread, though if refcount==1 or 0 it won't change
	int getRefCount() {
		int ret = 0;
		if(mutex) {
			lock();
			ret = *refCount;
			unlock(); // Here the other thread may change refcount, that's why it's approximate
		}
		return ret;
	}

	// Returns true only if the data is deleted (no other smartpointer used it), sets pointer to NULL then
	bool tryDeleteData() {
		if(mutex) {
			lock();
			if( *refCount == 1 )
			{
				unlock(); // Locks mutex again inside reset(), since we're only ones using data refcount cannot change from other thread
				reset();
				return true;	// Data deleted
			}
			unlock();
			return false; // Data not deleted
		}
		return true;	// Data was already deleted
	}

};

/*
template< typename _Obj>
class SmartObject : public SmartPointer< SmartObject<_Obj> > {
public:
	typedef SmartPointer< SmartObject<_Obj> > SmartPtrType;
	SmartObject() { SmartPtrType :: operator =( this ); }
	virtual ~SmartObject() {  }
private:
	// overwrite to disable this op=
	SmartPtrType& operator=(const SmartPtrType& pt) { assert(false); }
	
};
*/

#endif

