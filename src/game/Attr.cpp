//
//  Attr.cpp
//  OpenLieroX
//
//  Created by Albert Zeyer on 20.01.12.
//  code under LGPL
//

#include <map>
#include <set>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/bind/bind.hpp>
#include "Attr.h"
#include "util/macros.h"
#include "util/StaticVar.h"
#include "Debug.h"
#include "util/StringConv.h"
#include "gusanos/luaapi/classes.h"
#include "Game.h"
#include "GameState.h"
#include "CBytestream.h"
#include "OLXCommand.h"
#include "CClient.h"
#include "Mutex.h"
#include "CServerConnection.h"
#include "Debug.h"
#include "FindFile.h"
#include "LieroX.h"

using namespace boost::placeholders;

static CServerConnection* attrUpdateByClientScope = NULL;
static bool attrUpdateByServerScope = false;

AttrUpdateByClientScope::AttrUpdateByClientScope(CServerConnection *cl) {
	assert(cl != NULL);
	assert(attrUpdateByClientScope == NULL); // no nesting
	assert(!attrUpdateByServerScope);
	if(!cl->isLocalClient())
		attrUpdateByClientScope = cl;
}

AttrUpdateByClientScope::~AttrUpdateByClientScope() {
	attrUpdateByClientScope = NULL;
}

CServerConnection* AttrUpdateByClientScope::currentScope() {
	return attrUpdateByClientScope;
}

AttrUpdateByServerScope::AttrUpdateByServerScope() {
	assert(!attrUpdateByServerScope); // no nesting
	assert(attrUpdateByClientScope == NULL);
	attrUpdateByServerScope = true;
}

AttrUpdateByServerScope::~AttrUpdateByServerScope() {
	attrUpdateByServerScope = false;
}

typedef std::map<AttribRef, const AttrDesc*> AttrDescs;
static StaticVar<AttrDescs> attrDescs;

std::string AttrDesc::description() const {
	return std::string(LuaClassName(objTypeId)) + ":" + attrName;
}

void AttrDesc::set(BaseObject* base, const ScriptVar_t& v) const {
	assert(isStatic); // not yet implemented otherwise... we would need another dynamic function
	if(!authorizedToWrite(base)) return; // silently for now...
	pushObjAttrUpdate(*base, this);
	getValueScriptPtr(base).fromScriptVar(v);
}

bool AttrDesc::authorizedToWrite(const BaseObject* base) const {
	assert(base != NULL);
	if(this == Game::state_Type::attrDesc()) { // small exception for game.state
		if(game.state == Game::S_Quit && !bRestartGameAfterQuit) return false; // don't allow any changes anymore on game.state. just quit
		return true; // just allow any changes. client might want to disconnect, so client must be allowed to write this :)
	}
	if(game.state <= Game::S_Connecting) return true;
	if(!base->isRegistered()) return true;
	if(attrUpdateByServerScope || attrUpdateByClientScope) {
		if(game.isServer()) {
			assert(attrUpdateByClientScope);
			if(serverside) return false;
			return attrUpdateByClientScope == base->ownerClient();
		}
		else { // client
			assert(attrUpdateByServerScope);
			return true; // we just allow any updates from the server
		}
	}
	// About the const-cast: Too annoying to write in a clean way...
	if(game.isServer() && getAttrExt((BaseObject*) base).S2CupdateNeeded) return true;
	if(game.isClient() && cClient->getServerVersion() < OLXBetaVersion(0,59,10)) return true; // old protocol, we just manage it manually
	if(serverside) return game.isServer();
	if(!serverside) {
		if(game.isServer() && serverCanUpdate) return true;
		return base->weOwnThis();
	}
	return false;
}

bool AttrDesc::shouldUpdate(const BaseObject* base) const {
	if(!authorizedToWrite(base)) return false;
	if(game.isClient() && serverside) return false;
	return true;
}

AttribRef::AttribRef(const AttrDesc* attrDesc) {
	objTypeId = attrDesc->objTypeId;
	attrId = attrDesc->attrId;
}

void AttribRef::writeToBs(CBytestream* bs) const {
	bs->writeInt16(objTypeId);
	bs->writeInt16(attrId);
}

void AttribRef::readFromBs(CBytestream* bs) {
	objTypeId = bs->readInt16();
	attrId = bs->readInt16();
}

std::string AttribRef::description() const {
	const AttrDesc* attrDesc = getAttrDesc();
	if(attrDesc) return attrDesc->attrName;
	else return "<unknown attr " + to_string(objTypeId) + ":" + to_string(attrId) + ">";
}

AttribRef AttribRef::LowerLimit(ClassId c) {
	AttribRef r;
	r.objTypeId = c;
	r.attrId = 0;
	return r;
}

AttribRef AttribRef::UpperLimit(ClassId c) {
	AttribRef r;
	r.objTypeId = c;
	r.attrId = AttrDesc::AttrId(-1);
	return r;
}

const AttrDesc* AttribRef::getAttrDesc() const {
	AttrDescs::iterator it = attrDescs->find(*this);
	if(it != attrDescs->end()) return it->second;
	return NULL;
}

ObjAttrRef::ObjAttrRef(ObjRef o, const AttrDesc* attrDesc) {
	obj = o;
	attr = AttribRef(attrDesc);
}

void ObjAttrRef::writeToBs(CBytestream* bs) const {
	obj.writeToBs(bs);
	attr.writeToBs(bs);
}

void ObjAttrRef::readFromBs(CBytestream* bs) {
	obj.readFromBs(bs);
	attr.readFromBs(bs);
}

std::string ObjAttrRef::description() const {
	return "<" + obj.description() + "> " + attr.description();
}

ObjAttrRef ObjAttrRef::LowerLimit(ObjRef o) {
	ObjAttrRef r;
	r.obj = o;
	r.attr.objTypeId = 0;
	r.attr.attrId = 0;
	return r;
}

ObjAttrRef ObjAttrRef::UpperLimit(ObjRef o) {
	ObjAttrRef r;
	r.obj = o;
	r.attr.objTypeId = ClassId(-1);
	r.attr.attrId = AttrDesc::AttrId(-1);
	return r;
}

ScriptVar_t ObjAttrRef::get() const {
	assert(obj.obj);
	const BaseObject* oPt = obj.obj.get();
	assert(oPt != NULL); // or should we return the attr default?
	const AttrDesc* attrDesc = attr.getAttrDesc();
	assert(attrDesc != NULL);
	return attrDesc->get(oPt);
}


void registerAttrDesc(AttrDesc& attrDesc) {
	attrDescs.get()[AttribRef(&attrDesc)] = &attrDesc;
}

void iterAttrDescs(ClassId classId, bool withSuperClasses, boost::function<void(const AttrDesc* attrDesc)> callback) {
	AttrDescs::iterator itStart = attrDescs->lower_bound(AttribRef::LowerLimit(classId));
	AttrDescs::iterator itEnd = attrDescs->upper_bound(AttribRef::UpperLimit(classId));
	for(AttrDescs::iterator it = itStart; it != itEnd; ++it)
		callback(it->second);

	if(withSuperClasses) {
		const ClassInfo* classInfo = getClassInfo(classId);
		assert(classInfo != NULL);
		if(classInfo->superClassId != ClassId(-1))
			iterAttrDescs(classInfo->superClassId, withSuperClasses, callback);
	}
}

static void _addAttrDesc(std::vector<const AttrDesc*>& vec, const AttrDesc* attrDesc) {
	vec.push_back(attrDesc);
}

std::vector<const AttrDesc*> getAttrDescs(ClassId classId, bool withSuperClasses) {
	std::vector<const AttrDesc*> vec;
	boost::function<void(const AttrDesc* attrDesc)> callback = boost::bind(_addAttrDesc, boost::ref(vec), _1);
	iterAttrDescs(classId, withSuperClasses, callback);
	return vec;
}

const AttrDesc* findAttrDescByName(const std::string& name, ClassId classId, bool withSuperClasses) {
	std::vector<const AttrDesc*> attribs = getAttrDescs(classId, withSuperClasses);
	foreach(a, attribs) {
		if((*a)->attrName == name)
			return *a;
	}
	return NULL;
}


typedef std::vector< WeakRef<BaseObject> > ObjUpdates;
static StaticVar<ObjUpdates> objUpdates;
static StaticVar<Mutex> objUpdatesMutex;

static void pushObjAttrUpdate(BaseObject& obj) {
	objUpdates->push_back(obj.thisRef.obj);
}

void realCopyVar(ScriptVar_t& var);

#ifdef DEBUG
struct CallInfo {
	std::vector<void*> callstack;
	ScriptVar_t oldValue;
};
struct CallInfos {
	std::vector<CallInfo> callInfos;
	void push_back(const std::vector<void*>& callstack, ScriptVar_t curValue) {
		if(!callInfos.empty()) {
			if(callInfos.back().oldValue == curValue)
				callInfos.pop_back();
		}
		callInfos.push_back(CallInfo());
		callInfos.back().callstack = callstack;
		callInfos.back().oldValue = curValue;
	}
};
typedef std::map<ObjAttrRef, CallInfos> AttrUpdateCallinfos;
static StaticVar<AttrUpdateCallinfos> attrUpdateCallinfos;
#endif

static void attrUpdateDebugHookPrint(ObjAttrRef a, const ScriptVar_t& oldValue, const ScriptVar_t& newValue, const std::vector<void*>& callstack) {
	notes << "debugHook: <" << a.obj.description() << "> " << a.attr.getAttrDesc()->description() << ": update " << oldValue.toString() << " -> " << newValue.toString() << endl;
	DumpCallstack(StdoutPrintFct(), &callstack[0], (int)callstack.size());
}

static StaticVar<boost::shared_ptr<std::vector<std::string> > > debugAttrHookList;

static void attrUpdateDebugHook(ObjAttrRef a, const ScriptVar_t& oldValue, const ScriptVar_t& newValue, const std::vector<void*>& callstack) {
	if(debugAttrHookList.get().get() == NULL) {
		FILE* fp = OpenGameFile("debug_attrHookList.txt", "r");
		if(!fp) return;
		debugAttrHookList.get().reset(new std::vector<std::string>());
		while( !feof(fp) && !ferror(fp) ) {
			std::string l = ReadUntil(fp);
			TrimSpaces(l);
			if(l.empty()) continue;
			if(l[0] == '#') continue;

			debugAttrHookList.get()->push_back(l);
		}
		fclose(fp);
	}

	foreach(astr, *debugAttrHookList.get()) {
		if(a.attr.getAttrDesc()->attrName == *astr) {
			attrUpdateDebugHookPrint(a, oldValue, newValue, callstack);
		}
	}
}

static void attrUpdateDebugHooks() {
#ifdef DEBUG
	AttrUpdateCallinfos updates;
	{
		Mutex::ScopedLock lock(objUpdatesMutex.get());		
		std::swap(updates, attrUpdateCallinfos.get());
	}
	
	foreach(ci, updates) {
		ObjAttrRef a = ci->first;
		const ScriptVar_t* lastValue = NULL;
		const std::vector<void*>* lastCallstack = NULL;
		size_t i = 0;
		foreach(c, ci->second.callInfos) {
			if(i > 0)
				attrUpdateDebugHook(a, *lastValue, c->oldValue, *lastCallstack);
			lastValue = &c->oldValue;
			lastCallstack = &c->callstack;
			++i;
		}
		if(i > 0) {
			if(!a.obj.obj)
				attrUpdateDebugHook(a, *lastValue, ScriptVar_t(), *lastCallstack);
			else if(a.get() != *lastValue)
				attrUpdateDebugHook(a, *lastValue, a.get(), *lastCallstack);
		}
	}
#endif
}

static void attrUpdateAddCallInfo(BaseObject& obj, const AttrDesc* attrDesc) {
#ifdef DEBUG
	ScriptVar_t curValue = attrDesc->get(&obj);
	realCopyVar(curValue);
	std::vector<void*> callstack(128);
	int c = GetCallstack(0, &callstack[0], (int)callstack.size());
	if(c < 0) c = 0;
	callstack.resize(c);
	attrUpdateCallinfos.get()[ObjAttrRef(obj.thisRef, attrDesc)].push_back(callstack, curValue);
#endif
}

void pushObjAttrUpdate(BaseObject& obj, const AttrDesc* attrDesc) {
	if(!obj.isRegistered() && !attrDesc->onUpdate)
		// Ignore.
		// The level background cache loader (and other background loaders)
		// can trigger this, i.e. the LevelInfo attributes in infoForLevel().
		// This is done in another thread, so we *must* not continue here.
		// Note that iterAttrUpdates() might be desirable, e.g. for the
		// onUpdate handler, so we must be a bit restrictive.
		// This means, at the moment, we cannot update attributes with
		// onUpdate handlers in other threads.
		return;
	// Note that this is also important with respect to multithreading.
	// iterAttrUpdates() is always run on the main thread, and we do not
	// want it to handle objects which are managed by other threads.
	// E.g. in attrUpdateDebugHooks(), we can have
	// bool(a.obj.obj)==true, and then suddenly, bool(a.obj.obj)==false,
	// which will most likely crash.
	assert(!isGameloopThreadRunning() || isGameloopThread());
	
	// Note: The lock is problematic.
	// Even a ScriptVar_t copy can issue a new call into here.
	// Thus, we need to do certain things outside of the lock.
	attrUpdateAddCallInfo(obj, attrDesc);
	AttrUpdateInfo* updateInfo = NULL;

	{
		Mutex::ScopedLock lock(objUpdatesMutex.get());
		if(obj.attrUpdates.empty())
			pushObjAttrUpdate(obj);
		AttrExt& ext = attrDesc->getAttrExt(&obj);
		if(!ext.updated || obj.attrUpdates.empty()) {
			// Note that the move constructor of ScriptVar_t is important
			// here because attrUpdates is a vector which might issue
			// a copy.
			obj.attrUpdates.push_back(AttrUpdateInfo());
			ext.updated = true;
			updateInfo = &obj.attrUpdates.back();
		}
	}

	if(updateInfo) {
		updateInfo->attrDesc = attrDesc;
		updateInfo->oldValue = attrDesc->get(&obj);
		realCopyVar(updateInfo->oldValue);
	}
}

static void handleAttrUpdateLogging(BaseObject* oPt, const AttrDesc* attrDesc, ScriptVar_t oldValue) {
	// For now, just to console.
	// For some, no debug msg, too annoying.
	if(attrDesc->objTypeId == LuaID<CGameObject>::value && attrDesc->attrName == "vPos") return;
	if(attrDesc->objTypeId == LuaID<CGameObject>::value && attrDesc->attrName == "vVelocity") return;
	if(attrDesc->objTypeId == LuaID<Game>::value && attrDesc->attrName == "serverFrame") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "tState") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "iAFK") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "sAFKMessage") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "iCurrentWeapon") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "weaponSlots") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "iMoveDirectionSide") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "iFaceDirectionSide") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "fAngle") return;
	if(attrDesc->objTypeId == LuaID<CWorm>::value && attrDesc->attrName == "cNinjaRope") return;
	if(!oPt->thisRef)
		return; // obj not really registered
	notes << "<" << oPt->thisRef.description() << "> " << attrDesc->description() << ": update " << oldValue.toString() << " -> " << attrDesc->get(oPt).toString() << endl;
}

void iterAttrUpdates() {

	// We must do certain thinks outside of the objUpdatesMutex lock.
	// See also pushObjAttrUpdate() for reference.
	
	struct UpdateCallInfo {
		BaseObject* obj;
		const AttrDesc* attrDesc;
		ScriptVar_t oldValue;
		UpdateCallInfo(BaseObject* _o, const AttrDesc* _a, const ScriptVar_t& _v)
		: obj(_o), attrDesc(_a), oldValue(_v) {}
	};
	std::vector<UpdateCallInfo> updateCallbacks;

	attrUpdateDebugHooks();

	{
		Mutex::ScopedLock lock(objUpdatesMutex.get());

		foreach(o, objUpdates.get()) {
			BaseObject* oPt = o->get();
			if(oPt == NULL) continue; // object was deleted in the meanwhile

			foreach(u, oPt->attrUpdates) {
				const AttrDesc* const attrDesc = u->attrDesc;
				ScriptVar_t& oldValue = u->oldValue;

				attrDesc->getAttrExt(oPt).updated = false;
				// Note: In most cases, the attrib is static and this will not create
				// any temporary ScriptVar_t copy.
				// If it would, that would be dangerous because that can have many
				// side effects which might again access the object attrib system
				// - but we have te objUpdatesMutex locked here.
				if(attrDesc->compare(oPt, oldValue) == 0) continue;

				if(oPt->thisRef) // if registered
					game.gameStateUpdates->pushObjAttrUpdate(ObjAttrRef(oPt->thisRef, attrDesc));

				if(attrDesc->onUpdate)
					// We cannot call them here directly because we hold the objUpdatesMutex
					// (and must keep the hold), and the callbacks can likely access other
					// object attributes.
					updateCallbacks.push_back(UpdateCallInfo(oPt, attrDesc, oldValue));
				
				handleAttrUpdateLogging(oPt, attrDesc, oldValue);
			}

			oPt->attrUpdates.clear();
		}

		objUpdates->clear();
	}
		
	// Now call the onUpdate callbacks.
	for(UpdateCallInfo& update : updateCallbacks) {
		update.attrDesc->onUpdate(update.obj, update.attrDesc, update.oldValue);
	}
}


void dumpObject(const BaseObject* obj, CmdLineIntf* cliOut) {
	assert(obj != NULL);
	assert(obj->thisRef.classId != ClassId(-1));
	if(cliOut == NULL) cliOut = &stdoutCLI();

	cliOut->writeMsg(obj->thisRef.description() + " = {");
	std::vector<const AttrDesc*> attrDescs = getAttrDescs(obj->thisRef.classId, true);
	foreach(a, attrDescs) {
		cliOut->writeMsg("  " + (*a)->attrName + ": " + (*a)->get(obj).toString());
	}
	cliOut->writeMsg("}");
}

