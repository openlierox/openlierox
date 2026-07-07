#ifndef VERMES_OMFG_SCRIPT
#define VERMES_OMFG_SCRIPT

#include <string>
#include <istream>
#include <ostream>
#include <list>
#include <memory>
#include <vector>
#include <stdexcept>
#include <stdint.h>
#include "util/log.h"
#include <boost/crc.hpp>
#include <boost/shared_ptr.hpp>

class BaseAction;

namespace OmfgScript
{

template<class T>
struct Pimpl
{
protected:
	Pimpl(T* pimpl_)
	: pimpl(pimpl_)
	{
	}
	
	T* pimpl;
private:
	Pimpl(Pimpl const&);
	Pimpl operator=(Pimpl const&);
};

struct ParserImpl;
struct ActionFactoryImpl;
struct ParamDef;
struct ActionDef;
struct Function;

struct TokenBase
{
	typedef std::unique_ptr<TokenBase> ptr;
	
	TokenBase(Location loc_)
	: loc(loc_) 
	{
	}
	
	struct Type
	{
		enum type
		{
			Unknown,
			Default,
			Double,
			Int,
			String,
			List,
			Func,
		};
	};
	
	virtual ~TokenBase() {}
	
	virtual double toDouble() { loc.print("Object is not a double"); throw std::runtime_error("Object is not a double"); }
	virtual int toInt() {  loc.print("Object is not an integer"); throw std::runtime_error("Object is not an integer"); }
	virtual std::string const& toString() { loc.print("Object is not a string"); throw std::runtime_error("Object is not a string"); }
	virtual std::list<TokenBase*> const& toList() { loc.print("Object is not a list"); throw std::runtime_error("Object is not a list"); }
	virtual Function const* toFunction() { loc.print("Object is not a function"); throw std::runtime_error("Object is not a function"); }
	virtual std::ostream& output(std::ostream& s) { s << "<>"; return s; }
	
	bool isDouble() { return type() == Type::Double; }
	bool isInt() { return type() == Type::Int; }
	bool isString() { return type() == Type::String; }
	bool isList() { return type() == Type::List; }
	bool isFunction() { return type() == Type::Func; }
	bool isDefault() { return type() == Type::Default; }
	
	double toDouble(double def)
	{
		switch(type())
		{
			case Type::Double: case Type::Int:
				return toDouble();
			case Type::Default:
				return def;
				
			default: break;
		}
		
		loc.print("Expected numeric value");
		return def;
	}
	
	int toInt(int def)
	{
		switch(type())
		{
			case Type::Double: case Type::Int:
				return toInt();
			case Type::Default:
				return def;
				
			default: break;
		}
		
		loc.print("Expected integer value");
		return def;
	}
	
	std::list<TokenBase*> const& toList(std::list<TokenBase*> const& def)
	{
		switch(type())
		{
			case Type::List:
				return toList();
			case Type::Default:
				return def;
				
			default: break;
		}
		
		loc.print("Expected list");
		return def;
	}
	
	bool toBool(bool def)
	{
		return toInt(def ? 1 : 0) != 0;
	}
	
	uint32_t toColor(int r, int g, int b);
	
	std::string const& toString(std::string const& def)
	{
		switch(type())
		{
			case Type::String:
				return toString();
			case Type::Default:
				return def;
				
			default: break;
		}
		
		loc.print("Expected string value");
		return def;
	}
		
	bool assertList()
	{
		if(isList())
			return true;
		
		loc.print("Expected list");
		return false;
	}
	
	bool assertFunction()
	{
		if(isFunction())
			return true;
		
		loc.print("Expected function");
		return false;
	}
	
	bool assertString()
	{
		if(isString())
			return true;
		
		loc.print("Expected string value");
		return false;
	}
	
	virtual Type::type type()
	{ return Type::Default; }
	
	virtual void calcCRC(boost::crc_32_type& crc)
	{
		crc.process_byte(0xFF);
	}
	
	Location loc;
};

struct Function : public TokenBase
{
	std::string name;
	std::vector<TokenBase *> params;
	
	TokenBase* operator[](size_t i) const;
	
	virtual void calcCRC(boost::crc_32_type& crc);
	
protected:
	Function(Location loc_, std::string const& name_);
	
	~Function();

};

struct ParamProxy
{
	ParamProxy const& operator()(std::string const& name, bool optional = true) const;

	ParamProxy(ParamDef*);
	
	ParamDef* paramDef;
};

namespace ActionParamFlags
{
enum type
{
	Object = (1<<0),
	Object2 = (1<<1),
	Weapon = (1<<2),
};
};

struct ActionFactory : public Pimpl<ActionFactoryImpl>
{
	friend struct ParserImpl;
	ActionFactory();
	~ActionFactory();
	
	typedef BaseAction*(*CreateFunc)( std::vector<TokenBase* > const & );
	
	ParamProxy add(std::string const& name, CreateFunc, int requireMask);

	ActionDef* operator[](std::string const& name);
};

struct Parser : public Pimpl<ParserImpl>
{
	static TokenBase globalDefault;
	
	
	
	struct GameEventIter
	{
		friend struct Parser;
		
		~GameEventIter();
		
		GameEventIter& operator++();
		
		operator bool();
		
		int type();
		std::vector<TokenBase*> const& params();
		std::vector< boost::shared_ptr<BaseAction> >& actions();
		
		GameEventIter(Parser&);
		
	private:	
		void* data;
	};
	
	Parser(std::istream&, ActionFactory&, std::string const& fileName);
	
	~Parser();
	
	ParamProxy addEvent(std::string const& name, int type, int provideMask);
	
	bool run();
	
	double getDouble(std::string const& name, double def = 0.0);
	
	int getInt(std::string const& name, int def = 0);
	
	bool getBool(std::string const& name, bool def = false);
	
	std::string const& getString(std::string const& name, std::string const& def = "");
	
	std::list<TokenBase*> const& getList(std::string const& name, std::list<TokenBase*> const& def = std::list<TokenBase*>());
	
	Function const* getFunction(std::string const& name);
	
	Function const* getDeprFunction(std::string const& name);
	
	TokenBase* getRawProperty(std::string const& a);
	
	TokenBase* getProperty(std::string const& a);
		
	TokenBase* getProperty(std::string const& a, std::string const& b);
	
	TokenBase* getDeprProperty(std::string const& name);
	
	boost::crc_32_type::value_type getCRC();
	
	void crcProcessByte(unsigned char byte);
	
	bool incomplete();
	
	void error(std::string const&);
};

}

#endif //VERMES_OMFG_SCRIPT
