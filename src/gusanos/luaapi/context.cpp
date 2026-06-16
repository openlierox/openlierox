#include <cstdlib>
#include "context.h"
#include "Debug.h"

extern "C"
{
	#include "lualib.h"
	#include "lauxlib.h"
}


#include "gusanos/netstream.h"
#include "util/Bitstream.h"
#include "OLXCommand.h"
#include "CScriptableVars.h"
#include "gusanos/luaapi/classes.h"
#include "gusanos/lua/bindings.h"
#include "gusanos/LuaCallbacks.h"
#include "FindFile.h"
#include <cmath>
#include <map>
#include <set>
#include <boost/bind/bind.hpp>

#define FREELIST_REF 1
#define ARRAY_SIZE   2

using namespace boost::placeholders;

LuaContext luaIngame;
LuaContext luaGlobal;

int LuaContext::errorReport(lua_State* L)
{
	LuaContext context(L);
	
	char const* s = lua_tostring(context, 1);
	
	lua_Debug info;
	
	// Skip until the second colon
	char const* olds = s;
	for(int cc = 2; *s && ((*s != ':') || (--cc > 0)); ++s);
	
	if(!*s)
		s = olds;
	else
		s += 2; // Skip until beginning of message
	
	char const* lastname = NULL;
	
	for(int i = 1; lua_getstack(context, i, &info); ++i)
	{
		lua_getinfo (context, "Snl", &info);
		
		char const* name = info.name ? info.name : "N/A";
		
		if(i == 1)
			notes << info.source << ":" << info.currentline << ": " << s << endl;
		else
			notes << info.source << ":" << info.currentline << ": " << lastname << " called from here" << endl;
		
		lastname = name;
	}
	
	context.pushvalue(1);
	return 1;
}

bool LuaContext::logOnce(std::ostream& str)
{
	typedef std::map<std::string, std::set<int> > MapT;
	static MapT linesWarned;
	
	lua_Debug info;
	if(!lua_getstack(*this, 1, &info)) return false;
	lua_getinfo(*this, "Snl", &info);
	
	MapT::iterator i = linesWarned.find(info.source);
	if(i == linesWarned.end() || (i->second.find(info.currentline) == i->second.end()))
	{
		linesWarned[info.source].insert(info.currentline);
		str << info.source << ":" << info.currentline << ": ";
		return true;
	}
	return false;
}

void LuaContext::log(std::ostream& str)
{
	lua_Debug info;
	if(!lua_getstack(*this, 1, &info)) return;
	lua_getinfo(*this, "Snl", &info);
	
	str << info.source << ":" << info.currentline << ": ";
}

LuaContext::LuaContext() {}
LuaContext::LuaContext(lua_State* L) {
	if(L == luaIngame.weakRef.get())
		weakRef = luaIngame.weakRef;
	else if(L == luaGlobal.weakRef.get())
		weakRef = luaGlobal.weakRef;
	else
		assert(false);
}

namespace
{
	void* l_alloc (void*, void* ptr, size_t, size_t nsize)
	{
		if (nsize == 0)
		{
			free(ptr);
			return 0;
		}
		else
			return realloc(ptr, nsize);
	}
}

void LuaContext::init()
{
	weakRef.set(lua_newstate(l_alloc, 0));

	lua_pushinteger(*this, 3);
	lua_rawseti(*this, LUA_REGISTRYINDEX, ARRAY_SIZE);
	
	luaopen_base(*this);
	luaopen_table(*this);
	luaopen_string(*this);
	luaopen_math(*this);
}

void LuaContext::reset()
{
	close();
	init();
}

const char * LuaContext::istreamChunkReader(lua_State *L, void *data, size_t *size)
{
	static char buffer[1024];
	
	std::istream& stream = *(std::istream *)data;
	
	stream.read(buffer, 1024);

	*size = (size_t)stream.gcount();
	
	if(*size > 0)
		return buffer;
	else
		return 0;
}

void LuaContext::load(std::string const& chunk, std::istream& stream)
{
	lua_pushcfunction(*this, errorReport);
	int result = lua_load(*this, istreamChunkReader, &stream, chunk.c_str());
	
	if(result)
	{
		notes << "Lua error: " << lua_tostring(*this, -1) << endl;
		pop(2);
		return;
	}
	
	result = lua_pcall (*this, 0, 0, -2);
	
	switch(result)
	{
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
		{
			pop(1); // Pop error message
		}
		break;
	}
	pop(1); // Pop error function
}

typedef std::pair<std::string const*, int> StringData;

const char * stringChunkReader(lua_State *L, void *data, size_t *size)
{
	StringData& readData = *(StringData *)data;
	
	char const* ret = 0;
	
	switch(readData.second)
	{
		case 0:
			ret = "return (";
			*size = 8;
		break;
		
		case 1:
			ret = readData.first->data();
			*size = readData.first->size();
		break;
		
		case 2:
			ret = ")";
			*size = 1;
		break;
	}

	++readData.second;
	return ret;
}

int LuaContext::evalExpression(std::string const& chunk, std::string const& data)
{
	StringData readData(&data, 0);
	
	lua_pushcfunction(*this, errorReport);
	int result = lua_load(*this, stringChunkReader, &readData, chunk.c_str());
	
	if(result)
	{
		notes << "Lua error: " << this->tostring(-1) << endl;
		pop(2);
		return 0;
	}
	
	result = lua_pcall (*this, 0, 1, -2);
	
	switch(result)
	{
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
		{
			notes << "Lua error: " << this->tostring(-1) << endl;
			pop(2); // Pop error message and error function
			return 0;
		}
		break;
	}
	
	lua_remove(*this, -2); // Remove error function
	return 1;
}


int LuaContext::execCode(const std::string& data, CmdLineIntf &cli) {
	LuaCustomPrintScope printScope(*this, printFuncFromCLI(cli));

	lua_pushcfunction(*this, errorReport);
	int result = luaL_loadstring(*this, data.c_str());

	if(result) {
		cli.writeMsg("Lua error: " + std::string(this->tostring(-1)));
		pop(2);
		return 0;
	}

	result = lua_pcall (*this, 0, 1, -2);

	switch(result) {
	case LUA_ERRRUN:
	case LUA_ERRMEM:
	case LUA_ERRERR:
		cli.writeMsg("Lua error: " + std::string(this->tostring(-1)));
		pop(2); // Pop error message and error function
		return 0;
	}

	lua_remove(*this, -2); // Remove error function

	if(lua_isnoneornil(*this, -1)) {
		pop();
		return 0;
	}
	return 1;
}

typedef std::pair<std::istream*, int> IStreamData;

const char * istreamExprReader(lua_State *L, void *data, size_t *size)
{
	IStreamData& readData = *(IStreamData *)data;
	
	char const* ret = 0;
	
	switch(readData.second)
	{
		case 0:
			ret = "return (";
			*size = 8;
			++readData.second;
		break;
		
		case 1:
		{
			static char buffer[1024];
			readData.first->read(buffer, 1024);
			*size = (size_t)readData.first->gcount();
			if(!*size)
			{
				ret = ")";
				*size = 1;
				++readData.second;
			}
			else
				ret = buffer;
		}
		break;
	}

	return ret;
}

int LuaContext::evalExpression(std::string const& chunk, std::istream& stream)
{
	IStreamData readData(&stream, 0);
	
	lua_pushcfunction(*this, errorReport);
	int result = lua_load(*this, istreamExprReader, &readData, chunk.c_str());
	
	if(result)
	{
		notes << "Lua error: " << lua_tostring(*this, -1) << endl;
		pop(2);
		return 0;
	}
	
	result = lua_pcall (*this, 0, 1, -2);
	
	switch(result)
	{
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
		{
			notes << "Lua error: " << lua_tostring(*this, -1) << endl;
			pop(2); // Pop error message and error function
			return 0;
		}
		break;
	}
	
	lua_remove(*this, -2); // Remove error function
	return 1;
}

/*
void LuaContext::load(std::string const& chunk, istream& stream, string const& table)
{
	lua_pushcfunction(m_State, errorReport);
	int result = lua_load(m_State, istreamChunkReader, &stream, chunk.c_str());
	
	if(result)
	{
		cerr << "Lua error: " << lua_tostring(m_State, -1) << endl;
		return;
	}
	
	lua_pushstring(m_State, table.c_str());
	lua_rawget(m_State, LUA_GLOBALSINDEX);
	lua_setfenv(m_State, -2);
	
	//TODO: Create an error handler function that returns additional
	//debug info.
	result = lua_pcall (m_State, 0, 0, 0);
	
	switch(result)
	{
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
		{
			//TODO: throw lua_exception(*this);
			cerr << "Lua error: " << lua_tostring(m_State, -1) << endl;
		}
		break;
	}
}*/

int LuaContext::call(int params, int returns, int errfunc)
{
	int result = lua_pcall (*this, params, returns, errfunc);
		
	switch(result)
	{
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
		{
			//cerr << "Lua error: " << lua_tostring(m_State, -1) << endl;
			pop(1);
			return -1;
		}
		break;
	}
	
	return result;
}

/*
int LuaContext::callReference(LuaReference ref)
{
	pushReference(ref);
	int result = call(0, 0);
	
	return result;
}*/

void LuaContext::function(char const* name, lua_CFunction func, int upvalues)
{
	lua_pushcclosure(*this, func, upvalues);
	lua_setfield(*this, LUA_GLOBALSINDEX, name);
}

void LuaContext::tableFunction(char const* name, lua_CFunction func) // [0,0]
{
	lua_pushstring(*this, name);
	lua_pushcfunction(*this, func);
	lua_rawset(*this, -3);
}

static int getRegTable(lua_State* L, int index) { // [0,0]
	lua_rawgeti(L, LUA_REGISTRYINDEX, index);
	int ret = static_cast<int>(lua_tointeger(L, -1));
	lua_settop(L, -2);
	return ret;
}

static void setRegTable(lua_State* L, int index, int value) { // [0,0]
	lua_pushinteger(L, static_cast<lua_Integer>(value));
	lua_rawseti(L, LUA_REGISTRYINDEX, index);
}

LuaReference::Idx LuaContext::createReference() // [-1,0]
{
	assert(weakRef);

	// FREELIST_REF is a single-linked list of free RegistryTable slots.
	// It points to a free ref. If the value at the ref is non-zero, it again
	// points to another free ref, etc.
	// In destroyReference, we add entries to that list.

	int ref = getRegTable(*this, FREELIST_REF);

	if(ref != 0)
	{
		lua_rawgeti(*this, LUA_REGISTRYINDEX, ref);
		lua_rawseti(*this, LUA_REGISTRYINDEX, FREELIST_REF);
		// LuaRegistry[FREELIST_REF] = LuaRegistry[ref]
	}
	else
	{
		// LuaRegistry[ARRAY_SIZE] is initialized with 3. (LuaContext::init)
		ref = getRegTable(*this, ARRAY_SIZE);
		setRegTable(*this, ARRAY_SIZE, ref + 1);
	}

	lua_rawseti(*this, LUA_REGISTRYINDEX, ref);
	// LuaRegistry[ref] = StackTop
	return ref;
}

void LuaContext::assignReference(LuaReference::Idx ref) // [-1,0]
{
	if (ref > 2)
	{
		lua_rawseti(*this, LUA_REGISTRYINDEX, ref);
	}
	else
		pop(1); // Ignore it
}

void LuaContext::destroyReference(LuaReference::Idx ref)
{
	if (ref > 2)
	{
		lua_rawgeti(*this, LUA_REGISTRYINDEX, FREELIST_REF);
		lua_rawseti(*this, LUA_REGISTRYINDEX, ref);	/* t[ref] = t[FREELIST_REF] */
		setRegTable(*this, FREELIST_REF, ref);	/* t[FREELIST_REF] = ref */
	}
}

void LuaContext::pushReference(LuaReference::Idx ref)
{
	assert(ref);
	lua_rawgeti(*this, LUA_REGISTRYINDEX, ref);
}

namespace LuaType
{
enum type
{
	Nil = 0,
	Number,
	BooleanFalse,
	BooleanTrue,
	String,
	Table,
	UserData,
	End,
	Integer,
};
}

void LuaContext::serialize(BitStream& s, int i)
{
	switch(lua_type(*this, i))
	{
		case LUA_TNIL:
			s.addInt(LuaType::Nil, 4);
		break;
		
		case LUA_TNUMBER:
		{
			lua_Number n = lua_tonumber(*this, i);
			lua_Integer i = static_cast<lua_Integer>(n);
			if(std::fabs(i - n) < 0.00001)
			{
				s.addInt(LuaType::Integer, 4);
				s.addInt(i, 32);
			}
			else
			{
				s.addInt(LuaType::Number, 4);
				s.addFloat(static_cast<float>(n), 23);
			}
		}
		break;
		
		case LUA_TBOOLEAN:
		{
			int n = lua_toboolean(*this, i);
			s.addInt(n ? LuaType::BooleanTrue : LuaType::BooleanFalse, 4);
		}
		break;
		
		case LUA_TSTRING:
		{
			s.addInt(LuaType::String, 4);
			char const* n = lua_tostring(*this, i);
			s.addString(n);
		}
		break;
				
		case LUA_TTABLE:
		{
			s.addInt(LuaType::Table, 4);
			size_t idx = 1;
			for(;; ++idx)
			{
				lua_rawgeti(*this, i, idx);
				if(lua_isnil(*this, -1))
				{
					pop(1);
					break;
				}

				serialize(s, -1);
				pop(1);
			}
			s.addInt(LuaType::End, 4);
			
			lua_pushnil(*this);
			int tab = i < 0 ? i - 1 : i;
			while(lua_next(*this, tab) != 0)
			{
				if(!lua_isnumber(*this, -2) || lua_tointeger(*this, -2) >= (int)idx)
				{
					serialize(s, -2);
					serialize(s, -1);
				}
				pop(1);
			}
			s.addInt(LuaType::End, 4);
		}
		break;
		
		default: // Ignore any value we can't handle and encode a nil instead
			s.addInt(LuaType::Nil, 4);
		break;
	}
}

bool LuaContext::deserialize(BitStream& s)
{
	int t = s.getInt(4);
	switch(t)
	{
		case LuaType::Nil:
			lua_pushnil(*this);
		break;
		
		case LuaType::Number:
		{
			push(static_cast<lua_Number>(s.getFloat(23)));
		}
		break;
		
		case LuaType::Integer:
		{
			push(s.getInt(32));
		}
		break;
		
		case LuaType::BooleanTrue:
			push(true);
		break;
		
		case LuaType::BooleanFalse:
			push(false);
		break;
		
		case LuaType::String:
		{
			push(s.getString());				
		}
		break;
		
		case LuaType::Table:
		{
			lua_newtable(*this);
			
			for(int idx = 1; deserialize(s); ++idx)
			{
				lua_rawseti(*this, -2, idx);
			}
			
			while(deserialize(s))
			{
				if(!deserialize(s))
				{
					// Value was invalid
					pop(1); // Pop key
					return true; // Return what we have of the table
				}
				
				lua_rawset(*this, -3);
			}
		}
		break;
		
		default:
		{
			return false; // No value could be decoded
		}
		break;
	}
	
	return true;
}

void LuaContext::serialize(std::ostream& s, int i)
{
	switch(lua_type(*this, i))
	{
		case LUA_TNIL:
			s.put(LuaType::Nil);
		break;
		
		case LUA_TNUMBER:
		{
			lua_Number n = lua_tonumber(*this, i);
			s.put(LuaType::Number);
			s.write((char *)&n, sizeof(double));
		}
		break;
		
		case LUA_TBOOLEAN:
		{
			int n = lua_toboolean(*this, i);
			s.put(n ? LuaType::BooleanTrue : LuaType::BooleanFalse);
		}
		break;
		
		case LUA_TSTRING:
		{
			s.put(LuaType::String);
			size_t len;
			char const* n = lua_tolstring(*this, i, &len);
			s.put((len >> 24) & 0xFF);
			s.put((len >> 16) & 0xFF);
			s.put((len >> 8) & 0xFF);
			s.put(len & 0xFF);
			s.write(n, len);
		}
		break;
		
		case LUA_TTABLE:
		{
			s.put(LuaType::Table);
			lua_pushnil(*this);
			int tab = i < 0 ? i - 1 : i;
			while(lua_next(*this, tab) != 0)
			{
				serialize(s, -2);
				serialize(s, -1);
				pop(1);
			}
			s.put(LuaType::End);
		}
		break;
		
		default: // Ignore any value we can't handle and encode a nil instead
			s.put(LuaType::Nil);
		break;
	}
}

void LuaContext::deserialize(std::istream& s)
{
	int t = s.get();
	switch(t)
	{
		case LuaType::Nil:
			lua_pushnil(*this);
		break;
		
		case LuaType::Number:
		{
			lua_Number n;
			s.read((char *)&n, sizeof(double));
			push(n);
		}
		break;
		
		case LuaType::BooleanTrue:
			push(true);
		break;
		
		case LuaType::BooleanFalse:
			push(false);
		break;
		
		case LuaType::String:
		{
			int v1 = s.get();
			int v2 = s.get();
			int v3 = s.get();
			int v4 = s.get();
			size_t len = (v1 << 24) | (v2 << 16) | (v3 << 8) | v4;
			char* str = new char[len];
			try
			{
				s.read(str, len);
				lua_pushlstring(*this, str, len);
			}
			catch(...)
			{
				delete[] str;
				throw;
			}
			delete[] str;
		}
		break;
		
		case LuaType::Table:
		{
			lua_newtable(*this);
			while(s && s.peek() != LuaType::End)
			{
				deserialize(s);
				deserialize(s);
				lua_rawset(*this, -3);
			}
			s.get(); // Skip end
		}
		break;
	}
}

void LuaContext::serializeT(std::ostream& s, int i, int indent)
{
	switch(lua_type(*this, i))
	{
		case LUA_TNIL:
			s << "nil";
		break;
		
		case LUA_TNUMBER:
		{
			lua_Number n = lua_tonumber(*this, i);
			s << n;
		}
		break;
		
		case LUA_TBOOLEAN:
		{
			int n = lua_toboolean(*this, i);
			s << (n ? "true" : "false");
		}
		break;
		
		case LUA_TSTRING:
		{
			char const* n = lua_tostring(*this, i);
			s << '"';
			
			for(char const* p = n; *p; ++p)
			{
				char c = *p;
				if(c == '"')
					s << "\\\"";
				else
					s << c;
			}
			s << '"';
		}
		break;
		
		case LUA_TTABLE:
		{
			s << "{\n";
			++indent;
			size_t idx = 1;
			for(;; ++idx)
			{
				lua_rawgeti(*this, i, idx);
				if(lua_isnil(*this, -1))
				{
					pop(1);
					break;
				}
				
				for(int j = 0; j < indent; ++j)
					s.put('\t');
				serializeT(s, -1);
				s << ";\n";
				pop(1);
			}
			
			lua_pushnil(*this);
			int tab = i < 0 ? i - 1 : i;
			while(lua_next(*this, tab) != 0)
			{
				if(!lua_isnumber(*this, -2) || lua_tointeger(*this, -2) >= (int)idx)
				{
					for(int j = 0; j < indent; ++j)
						s.put('\t');
					s << '[';
					serializeT(s, -2, indent);
					s << "] = ";
					serializeT(s, -1, indent);
					s << ";\n";
				}
				pop(1);
			}
			--indent;
			for(int j = 0; j < indent; ++j)
				s.put('\t');
			s << '}';
		}
		break;
		
		default: // Ignore any value we can't handle and encode a nil instead
			s << "nil";
		break;
	}
}

std::string LuaContext::convert_tostring(int i) {
	assert(i != 0);
	int luaTop = lua_gettop(*this);
	if(i < 0) {
		if(-i <= luaTop)
			i = luaTop + i + 1; // make it absolute
	}
	else if(i > 0) {
		assert(i <= luaTop);
	}

	lua_getfield(*this, LUA_GLOBALSINDEX, "tostring");
	lua_pushvalue(*this, i);
	lua_call(*this, 1, 1);

	const char* s = lua_tostring(*this, -1);
	std::string ret = "<NULL>";
	if(s) ret = s;
	pop();

	return ret;
}

LuaContext& LuaContext::pushScriptVar(const ScriptVar_t& var) {
	switch(var.type) {
	case SVT_BOOL: push(bool(var)); break;
	case SVT_INT32: push(int(var)); break;
	case SVT_UINT64: push(int(var)); break;
	case SVT_FLOAT: push(float(var)); break;
	case SVT_STRING: push(std::string(var)); break;
	case SVT_CUSTOM:
		// Note: no Lua reference because it would reference to
		// a maybe temporary var reference.
		push(var.toString());
		break;
	case SVT_CustomWeakRefToStatic:
		// A static CustomVar should always we writeable, thus we
		// can cast here.
		push(((CustomVar*)var.customVar())->getLuaReference());
		break;
	default:
		push(var.toString());
	}

	return *this;
}

Result LuaContext::toScriptVar(int idx, ScriptVar_t& var) {
	if(lua_isnoneornil(*this, idx))
		return "value is none or nil";

	if(lua_isboolean(*this, idx)) {
		var = ScriptVar_t(tobool(idx));
		return true;
	}

	if(lua_isnumber(*this, idx)) {
		lua_Number n = lua_tonumber(*this, idx);
		if(n == lua_Number(int32_t(n))) { // int32_t
			var = ScriptVar_t(int32_t(n));
			return true;
		}

		var = ScriptVar_t(float(n));
		return true;
	}

	if(lua_isstring(*this, idx)) {
		var = ScriptVar_t(tostring(idx));
		return true;
	}

	if(BaseObject* obj = getObject<BaseObject>(*this, idx)) {
		CustomVar* customVar = dynamic_cast<CustomVar*>(obj);
		if(!customVar)
			return obj->thisRef.description() + " not supported";
		var = ScriptVar_t(obj->thisRef.obj);
		return true;
	}

	return getLuaTypename(idx) + " not supported";
}


void LuaContext::close()
{
	if(weakRef) {
		lua_close(*this);
		weakRef.overwriteShared(NULL);
	}
}

LuaContext::~LuaContext()
{
	// We might not always use the global instance but have a local one,
	// probably via the LuaContext(lua_State*) constructor.
	// In that case, it would almost certainly be wrong to close the m_State here.
	// Thus, we don't do it. Thus, lua.close() must be called manually.
}

static int luaPrintFromScope(lua_State* L) {
	LuaCustomPrintScope& scope = **(LuaCustomPrintScope**)lua_touserdata(L, lua_upvalueindex(1));
	return scope.printFunc(L);
}

LuaCustomPrintScope::LuaCustomPrintScope(LuaContext& context_, LuaCustomPrintScope::Func printFunc_)
	: context(context_), printFunc(printFunc_) {
	{
		// save old print func
		lua_getfield(context, LUA_GLOBALSINDEX, "print");
		oldPrintFunc.create(context);
	}
	{
		// push print-function-wrapper
		void** p = (void**)lua_newuserdata_init(context, sizeof(void*));
		*p = this;
		lua_pushcclosure(context, luaPrintFromScope, 1);

		// and replace print func
		lua_setfield(context, LUA_GLOBALSINDEX, "print");
	}
}

LuaCustomPrintScope::~LuaCustomPrintScope() {
	// restore old print func
	context.push(oldPrintFunc);
	oldPrintFunc.destroy();
	lua_setfield(context, LUA_GLOBALSINDEX, "print");
}

static int luaPrintOnCLI(CmdLineIntf& cli, lua_State* L)
{
	std::stringstream str;
	str << "Lua: ";

	int c = lua_gettop(L);
	for(int i = 1; i <= c; ++i)
	{
		if(const char* s = lua_tostring(L, i))
			str << s;
	}

	cli.writeMsg(str.str());
	return 0;
}

LuaCustomPrintScope::Func printFuncFromCLI(CmdLineIntf& cli) {
	using namespace boost;
	return boost::bind(luaPrintOnCLI, ref(cli), _1);
}

void LuaReference::cleanup() {
	assert(idxs.get());
	for(IdxMap::iterator i = idxs->begin(); i != idxs->end();) {
		IdxMap::iterator next = i; ++next;
		if(!i->first)
			idxs->erase(i);
		i = next;
	}
}

void LuaReference::create(LuaContext& ctx) {
	assert(!isNilRef);
	assert(idxs.get());
	assert(idxs->count(ctx.weakRef) == 0); // not yet created for this Lua context
	Idx idx = ctx.createReference();
	idxs->insert(std::make_pair(ctx.weakRef, idx));
	cleanup();
}

void LuaReference::push(LuaContext& ctx) const {
	if(isNilRef) {
		lua_pushnil(ctx);
		return;
	}
	assert(idxs.get());
	IdxMap::iterator i = idxs->find(ctx.weakRef);
	assert(i != idxs->end());
	ctx.pushReference(i->second);
}

bool LuaReference::isSet(const LuaContext& ctx) const {
	if(isNilRef) return true;
	assert(idxs.get());
	assert(ctx);
	IdxMap::iterator i = idxs->find(ctx.weakRef);
	return i != idxs->end();
}

void LuaReference::destroy() {
	if(isNilRef) return;
	assert(idxs.get());
	for(IdxMap::iterator i = idxs->begin(); i != idxs->end(); ++i) {
		if(!i->first) continue;
		LuaContext context(i->first.get());
		context.destroyReference(i->second);
	}
	idxs->clear();
}

void LuaReference::invalidate() {
	if(isNilRef) return;
	assert(idxs.get());
	for(IdxMap::iterator i = idxs->begin(); i != idxs->end(); ++i) {
		if(!i->first) continue;
		LuaContext context(i->first.get());
		lua_pushnil(context);
		context.assignReference(i->second);
	}
	idxs->clear();
}



static void luaGlobalScript(const std::string& path) {
	std::ifstream f;
	OpenGameFileR(f, path, std::ios::binary | std::ios::in);
	if(!f) {
		notes << "luaGlobalScript: " << path << " not found" << endl;
		return;
	}

	luaGlobal.load(path, f);
}

void initLuaGlobal() {
	luaGlobal.init();
	LuaBindings::init(luaGlobal);

	for_each_iterator(std::string, s, FileListIter(".", false, -1, "startup*.lua")) {
		notes << "running Lua startup script " << s->get() << endl;
		luaGlobalScript(s->get());
	}
}

void quitLuaGlobal() {
	LUACALLBACK(exit).call()();
	luaGlobal.close();
}

