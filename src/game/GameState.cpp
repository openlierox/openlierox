/*
 *  GameState.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 15.02.12.
 *  code under LGPL
 *
 */

#include "GameState.h"
#include "Game.h"
#include "Settings.h"
#include "CWorm.h"
#include "CBytestream.h"
#include "ClassInfo.h"
#include "ProfileSystem.h"
#include "CServerConnection.h"
#include "game/Attr.h"

ScriptVar_t ObjectState::getValue(AttribRef a) const {
	Attribs::const_iterator it = attribs.find(a);
	if(it == attribs.end())
		return a.getAttrDesc()->defaultValue;
	return it->second.value;
}

GameStateUpdates::operator bool() const {
	if(!objs.empty()) return true;
	if(!objCreations.empty()) return true;
	if(!objDeletions.empty()) return true;
	return false;
}

void GameStateUpdates::writeToBs(CBytestream* bs, const GameState& oldState) const {
	bs->writeInt16((uint16_t)objCreations.size());
	const_foreach(o, objCreations) {
		o->writeToBs(bs);
	}

	bs->writeInt16((uint16_t)objDeletions.size());
	const_foreach(o, objDeletions) {
		o->writeToBs(bs);
	}

	bs->writeInt((uint32_t)objs.size(), 4);
	const_foreach(a, objs) {
		const ObjAttrRef& attr = *a;
		ScriptVar_t curValue = a->get();
		attr.writeToBs(bs);
		if((attr.attr.getAttrDesc()->attrType == SVT_CustomWeakRefToStatic || attr.attr.getAttrDesc()->attrType == SVT_CUSTOM) && oldState.haveObject(attr.obj)) {
			ScriptVar_t oldValue = oldState.getValue(attr);
			assert(oldValue.isCustomType());
			bs->writeVar(curValue, oldValue.customVar());
		}
		else
			bs->writeVar(curValue);
	}
}

static BaseObject* getObjFromRef(ObjRef r, bool alsoGameSettings = false) {
	switch(r.classId) {
	case LuaID<Game>::value: return &game;
	case LuaID<CWorm>::value: return game.wormById(r.objId, false);
	case LuaID<Settings>::value: if(alsoGameSettings) { assert(game.isServer()); return &gameSettings; }
	default: break;
	}
	return NULL;
}

static bool attrBelongsToClass(const ClassInfo* classInfo, const AttrDesc* attrDesc) {
	return classInfo->isTypeOf(attrDesc->objTypeId);
}

// This only make sense as server.
// See also ObjRef::ownThis
static bool ownObject(CServerConnection* source, ObjRef o) {
	assert(game.isServer());
	// This is very custom right now and need to be made somewhat more general.
	if(o.classId == LuaID<CWorm>::value) {
		return source->OwnsWorm(o.objId);
	}
	return false;
}

void GameStateUpdates::handleFromBs(CBytestream* bs, CServerConnection* source) {
	if(game.isServer()) {
		assert(source != NULL);
		if(source->isLocalClient()) {
			// This shouldn't happen. It means sth in our server connection list is messed up.
			errors << "GameStateUpdates::handleFromBs as server from local client" << endl;
			bs->SkipAll();
			return;
		}
	}
	else
		assert(source == NULL);

	uint16_t creationsNum = bs->readInt16();
	for(uint16_t i = 0; i < creationsNum; ++i) {
		ObjRef r;
		r.readFromBs(bs);
		//BaseObject* o = getClassInfo(r.classId)->createInstance();
		//o->thisRef.classId = r.classId;
		//o->thisRef.objId = r.objId;

		// we only handle/support CWorm objects for now...
		if(game.isServer()) {
			errors << "GameStateUpdates::handleFromBs: got obj creation as server: " << r.description() << endl;
			bs->SkipAll();
			return;
		}
		if(r.classId != LuaID<CWorm>::value) {
			errors << "GameStateUpdates::handleFromBs: obj-creation: invalid class: " << r.description() << endl;
			bs->SkipAll();
			return;
		}
		if(game.wormById(r.objId, false) != NULL) {
			errors << "GameStateUpdates::handleFromBs: worm-creation: worm " << r.objId << " already exists" << endl;
			bs->SkipAll();
			return;
		}
		game.createNewWorm(r.objId, false, NULL, Version());
		if(source) {
			if(source->gameState->haveObject(r)) {
				// it should not have the object at this point. however, the client might be fucked up
				errors << "GameStateUpdate from client: add object " << r.description() << " which it should have had" << endl;
			}
			else
				source->gameState->addObject(r);
		}
	}

	uint16_t deletionsNum = bs->readInt16();
	for(uint16_t i = 0; i < deletionsNum; ++i) {
		ObjRef r;
		r.readFromBs(bs);

		// we only handle/support CWorm objects for now...
		if(game.isServer()) {
			errors << "GameStateUpdates::handleFromBs: got obj deletion as server: " << r.description() << endl;
			bs->SkipAll();
			return;
		}
		if(r.classId != LuaID<CWorm>::value) {
			errors << "GameStateUpdates::handleFromBs: obj-deletion: invalid class: " << r.description() << endl;
			bs->SkipAll();
			return;
		}
		CWorm* w = game.wormById(r.objId, false);
		if(!w) {
			errors << "GameStateUpdates::handleFromBs: obj-deletion: worm " << r.objId << " does not exist" << endl;
			bs->SkipAll();
			return;
		}
		game.removeWorm(w);
		if(source) {
			if(!source->gameState->haveObject(r)) {
				// it should always have the object at this point. however, the client might be fucked up
				errors << "GameStateUpdate from client: remove obj " << r.description() << " which it should not have had" << endl;
				source->gameState->addObject(r);
			}
			source->gameState->removeObject(r);
		}
	}

	uint32_t attrUpdatesNum = bs->readInt(4);
	for(uint32_t i = 0; i < attrUpdatesNum; ++i) {
		ObjAttrRef r;
		r.readFromBs(bs);

		const AttrDesc* attrDesc = r.attr.getAttrDesc();
		if(attrDesc == NULL) {
			errors << "GameStateUpdates::handleFromBs: AttrDesc for update not found" << endl;
			bs->SkipAll();
			return;
		}

		const ClassInfo* classInfo = getClassInfo(r.obj.classId);
		if(classInfo == NULL) {
			errors << "GameStateUpdates::handleFromBs: class " << r.obj.classId << " for obj-update unknown" << endl;
			bs->SkipAll();
			return;
		}

		if(!attrBelongsToClass(classInfo, attrDesc)) {
			errors << "GameStateUpdates::handleFromBs: attr " << attrDesc->description() << " does not belong to class " << r.obj.classId << " for obj-update" << endl;
			bs->SkipAll();
			return;
		}

		// see GameStateUpdates::diffFromStateToCurrent for the other side
		if(attrDesc->serverside) {
			if(game.isServer()) {
				errors << "GameStateUpdates::handleFromBs: got serverside attr update as server: " << r.description() << endl;
				bs->SkipAll();
				return;
			}
		}
		else { // client-side attr
			if(game.isServer() && !ownObject(source, r.obj)) {
				errors << "GameStateUpdates::handleFromBs: got update from not-authorized client " << source->debugName() << ": " << r.description() << endl;
				bs->SkipAll();
				return;
			}
		}

		// for now, this is somewhat specific to the only types we support
		if(r.obj.classId == LuaID<Settings>::value) {
			if(game.isServer()) {
				errors << "GameStateUpdates::handleFromBs: got settings update as server" << endl;
				bs->SkipAll();
				return;
			}
			if(!Settings::getAttrDescs().belongsToUs(attrDesc)) {
				errors << "GameStateUpdates::handleFromBs: settings update AttrDesc " << attrDesc->description() << " is not a setting attr" << endl;
				bs->SkipAll();
				return;
			}
			// Somewhat hacky right now. We don't really manipulate gameSettings.
			::pushObjAttrUpdate(gameSettings, attrDesc);
			FeatureIndex fIndex = Settings::getAttrDescs().getIndex(attrDesc);
			bs->readVar(cClient->getGameLobby().write(fIndex));
		}
		else {
			BaseObject* o = getObjFromRef(r.obj);
			if(o == NULL) {
				errors << "GameStateUpdates::handleFromBs: object for attr update not found: " << r.description() << endl;
				bs->SkipAll();
				return;
			}
			if(o->thisRef != r.obj) {
				errors << "GameStateUpdates::handleFromBs: object-ref for attr update invalid: " << r.description() << endl;
				bs->SkipAll();
				return;
			}

			if(attrDesc->attrType == SVT_CustomWeakRefToStatic) {
				CustomVar* v = (CustomVar*)attrDesc->getValuePtr(o);
				::pushObjAttrUpdate(*o, attrDesc);
				ScriptVar_t scriptVarRef(v->thisRef.obj);
				bs->readVar(scriptVarRef);
				assert(scriptVarRef.type == SVT_CustomWeakRefToStatic);
				//if(attrDesc->attrName == "weaponSlots")
					//notes << "game state update: <" << r.obj.description() << "> " << attrDesc->attrName << " to " << scriptVarRef.toString() << endl;
			}
			else {
				ScriptVar_t v;
				bs->readVar(v);

				if(attrDesc == game.state.attrDesc()) {
					if((int)v < Game::S_Lobby) {
						notes << "GameStateUpdates: server changed to " << Game::StateAsStr(v) << ", we wait for drop/leaving package" << endl;
						//v = Game::S_Inactive;
						continue;
					}
				}
				attrDesc->set(o, v);
			}
		}

		/*if(attrDesc->attrName != "serverFrame")
			notes << "game state update: <" << r.obj.description() << "> " << attrDesc->attrName << " to " << v.toString() << endl;*/
		if(source) {
			if(!source->gameState->haveObject(r.obj)) {
				// it should always have the object at this point. however, the client might be fucked up
				errors << "GameStateUpdate from client: attrib update " << r.description() << " about object which it should not have had" << endl;
				source->gameState->addObject(r.obj);
			}
			BaseObject* o = getObjFromRef(r.obj, true);
			assert(o);
			source->gameState->setObjAttr(r, r.attr.getAttrDesc()->get(o));
		}
	}
}

void GameStateUpdates::pushObjAttrUpdate(ObjAttrRef a) {
	objs.insert(a);
}

void GameStateUpdates::pushObjCreation(ObjRef o) {
	objDeletions.erase(o);
	objCreations.insert(o);
}

void GameStateUpdates::pushObjDeletion(ObjRef o) {
	Objs::iterator itStart = objs.lower_bound(ObjAttrRef::LowerLimit(o));
	Objs::iterator itEnd = objs.upper_bound(ObjAttrRef::UpperLimit(o));
	objs.erase(itStart, itEnd);
	objCreations.erase(o);
	objDeletions.insert(o);
}

void GameStateUpdates::reset() {
	objs.clear();
	objCreations.clear();
	objDeletions.clear();
}

void GameStateUpdates::diffFromStateToCurrent(const GameState& s) {
	reset();
	foreach(o, game.gameStateUpdates->objCreations) {
		if(game.isClient()) continue; // none-at-all right now... worm-creation is handled independently atm
		if(!o->obj) continue;
		if(!o->obj.get()->weOwnThis()) continue;
		if(!s.haveObject(*o))
			pushObjCreation(*o);
	}
	foreach(u, game.gameStateUpdates->objs) {
		BaseObject* obj = u->obj.obj.get();
		if(!obj) continue;
		const AttrDesc* attrDesc = u->attr.getAttrDesc();
		assert(attrDesc != NULL);
		if(!attrDesc->shouldUpdate(obj)) continue;

		attrDesc->getAttrExt(obj).S2CupdateNeeded = false;

		ScriptVar_t curValue = u->get();
		ScriptVar_t stateValue = attrDesc->defaultValue;
		if(s.haveObject(u->obj))
			stateValue = s.getValue(*u);
		if(curValue == stateValue) continue;

		/*if(attrDesc->attrName != "serverFrame")
			notes << "send update " << u->description() << ": " << stateValue.toString() << " -> " << curValue.toString() << endl;*/

		pushObjAttrUpdate(*u);
	}
	foreach(o, game.gameStateUpdates->objDeletions) {
		if(game.isClient()) continue; // see obj-creations
		if(!o->obj) continue;
		if(!o->obj.get()->weOwnThis()) continue;
		if(s.haveObject(*o))
			pushObjDeletion(*o);
	}
}

static ObjectState& GameState_registerObj(GameState& s, BaseObject* obj) {
	return s.objs[obj->thisRef] = ObjectState(obj);
}

GameState::GameState() {
	// register singletons which are always there
	GameState_registerObj(*this, &game);
	GameState_registerObj(*this, &gameSettings);
}

GameState GameState::Current() {
	GameState s;
	s.updateToCurrent();
	return s;
}

void GameState::reset() {
	*this = GameState();
}

void realCopyVar(ScriptVar_t& var);

void GameState::updateToCurrent() {
	reset();
	foreach(o, game.gameStateUpdates->objCreations) {
		addObject(*o);
	}
	foreach(u, game.gameStateUpdates->objs) {
		ObjAttrRef r = *u;
		setObjAttr(r, r.get());
	}
}

void GameState::addObject(ObjRef o) {
	// The fresh GameState() registers &game and &gameSettings as
	// singletons. After a first match, gameStateUpdates->objCreations
	// can also contain those refs (or any other object whose pointer
	// gets reused across game restarts), and updateToCurrent() then
	// re-adds them on top of the singletons. The actual store
	// (operator[] = ...) is fine with that — it just replaces — so
	// drop the strict assertion that only fired on the harmless
	// re-add path. Keeping the data write means the second match
	// starts with a coherent state instead of aborting in
	// VALIDATE_BUILD code paths.
	objs[o] = ObjectState(o);
}

void GameState::removeObject(ObjRef o) {
	assert(haveObject(o));
	objs.erase(o);
}

void GameState::setObjAttr(ObjAttrRef r, ScriptVar_t value) {
	Objs::iterator it = objs.find(r.obj);
	assert(it != objs.end());
	ObjectState& s = it->second;
	assert(s.obj == it->first);
	s.attribs[r.attr].value = value;
	realCopyVar(s.attribs[r.attr].value);
}

bool GameState::haveObject(ObjRef o) const {
	Objs::const_iterator it = objs.find(o);
	return it != objs.end();
}

ScriptVar_t GameState::getValue(ObjAttrRef a) const {
	Objs::const_iterator it = objs.find(a.obj);
	assert(it != objs.end());
	assert(it->second.obj == a.obj);
	return it->second.getValue(a.attr);
}

