/*
 *  SettingsPreset.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 24.02.10.
 *  code under LGPL
 *
 */

#include <boost/bind/bind.hpp>
#include "SettingsPreset.h"
#include "Mod.h"
#include "CGameScript.h"
#include "FindFile.h"
#include "Options.h"
#include "util/macros.h"
#include "ConfigHandler.h"
#include "DeprecatedGUI/CCombobox.h"
#include "gui/List.h"
#include "StringUtils.h"

static GameSettingsPresetInfo settingsInfo(const std::string& path, bool global) {
	GameSettingsPresetInfo info;
	info.global = global;
	if(!ReadString(path, "GameSettings", "Name", info.name.write(), ""))
		info.name = GetBaseFilenameWithoutExt(path);
	ReadString(path, "GameSettings", "Description", info.description, "");
	info.path = path;
	return info;
}

std::set<GameSettingsPresetInfo> presetsForMod(const std::string& modDir) {
	std::set<GameSettingsPresetInfo> presets;
	for(Iterator<std::string>::Ref it = FileListIter(".", false, FM_REG, "*.gamesettings"); it->isValid(); it->next())
		presets.insert(settingsInfo("./" + GetBaseFilename(it->get()), true));
	
	for(Iterator<std::string>::Ref it = FileListIter(modDir, false, FM_REG, "*.gamesettings"); it->isValid(); it->next())
		presets.insert(settingsInfo(modDir + "/" + GetBaseFilename(it->get()), false));
	
	return presets;
}

GuiListItem::Pt infoForSettingsPreset(const GameSettingsPresetInfo& preset) {
	struct Item : GuiListItem {
		Item(const GameSettingsPresetInfo& p) : preset(p) {}
		GameSettingsPresetInfo preset;
		
		virtual std::string caption() { return preset.name; }
		virtual std::string tooltip() { return preset.description; }		
		virtual std::string index() { return preset.path; }		
	};
	
	return new Item(preset);
}

static GuiItemList getCurrentSettingsPresetList() {
	std::set<GameSettingsPresetInfo> presets = presetsForMod(gameSettings[FT_Mod].as<ModInfo>()->path);
	GuiItemList presetList = new GuiItemList::value_type();
	foreach(i, presets)
	presetList->push_back(infoForSettingsPreset(*i));
	return presetList;
}

GuiList::Pt dynamicPresetListForCurrentMod() {
	return dynamicGuiList(boost::bind(&getCurrentSettingsPresetList));
}

void setupModGameSettingsPresetComboboxes(DeprecatedGUI::CCombobox* modList, DeprecatedGUI::CCombobox* presetList) {
	presetList->setListBackend( dynamicPresetListForCurrentMod() );
	modList->OnChangeSelection.connect( boost::bind(&DeprecatedGUI::CCombobox::updateFromListBackend, presetList) );
	presetList->updateFromListBackend(); // also update right now
}


GameSettingsPresetInfo GameSettingsPresetInfo::Default() {
	GameSettingsPresetInfo i;
	i.name = "Standard";
	i.path = "./Standard.gamesettings";
	i.global = true;
	return i;
}


GameSettingsPresetInfo::GameSettingsPresetInfo() : global(true) {
	thisRef.classId = LuaID<GameSettingsPresetInfo>::value;
}

bool GameSettingsPresetInfo::operator==(const CustomVar& o) const {
	const GameSettingsPresetInfo* oi = dynamic_cast<const GameSettingsPresetInfo*> (&o);
	return oi && (*oi == *this);
}

bool GameSettingsPresetInfo::operator<(const CustomVar& o) const {
	const GameSettingsPresetInfo* oi = dynamic_cast<const GameSettingsPresetInfo*> (&o);
	if(oi) return *this < *oi;
	return this < &o;
}

std::string GameSettingsPresetInfo::toString() const {
	return path;
}

bool GameSettingsPresetInfo::fromString( const std::string & str) {
	bool global = GetBaseFilename(str) == str || strStartsWith(str, "./");
	*this = settingsInfo(str, global);
	return true;
}

REGISTER_CLASS(GameSettingsPresetInfo, LuaID<CustomVar>::value)
