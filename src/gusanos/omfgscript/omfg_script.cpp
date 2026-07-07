#include "omfg_script.h"
#include "util/macros.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <iostream>
#include "util/stringbuild.h"
#include "util/text.h"
#include "gusanos/allegro.h"
#include <boost/crc.hpp>
using std::unique_ptr;
using std::cout;
using std::endl;

class BaseAction;

namespace OmfgScript
{
	
struct ParamDef
{
	struct Param
	{
		Param(std::string const& name_, bool optional_ = true)
		: name(name_), optional(optional_)
		{
		}
		
		std::string name;
		bool optional;
	};
	
	//ParamDef& operator()(std::string const& name_)
	void addParam(std::string const& name, bool optional = true)
	{
		params.push_back(Param(name, optional));
		//return *this;
	}
	
	std::vector<Param> params;
};

struct Parameters
{
	enum
	{
		PosParamAfterNamed = (1<<0),
		MaxReachParam = (1<<1),
		Error = (1<<2),
	};
	
	Parameters(ParamDef* def, Location loc_)
	: cur(0)
	, flags(0), paramDef(def), params(def->params.size(), 0), loc(loc_)
	{
	}
	
	~Parameters()
	{
		foreach(i, params)
		{
			delete *i;
		}
	}
	
	void addParam(std::string const& name, unique_ptr<OmfgScript::TokenBase> v, Location loc_)
	{
		size_t i = 0;
		for(; i < paramDef->params.size(); ++i)
		{
			if(paramDef->params[i].name == name)
				break;
		}
		
		if(i >= paramDef->params.size())
		{
			int minDist = 1000000;
			int minParam = -1;
			for(i = 0; i < paramDef->params.size(); ++i)
			{
				if(!params[i])
				{
					int dist = levenshteinDistance(name, paramDef->params[i].name);
					if(dist < minDist)
					{
						minDist = dist;
						minParam = (int)i;
					}
				}
			}
			
			if(minParam >= 0 && minDist <= ((int)name.size() + 1) / 2)
				error("Unknown parameter '" + name + "'. Did you mean '" + paramDef->params[minParam].name + "'?", loc_);
			else
				error("Unknown parameter '" + name + "'", loc_);
			return; //TODO: Error
		}
		
		if(params[i])
		{
			error("Parameter '" + name + "' already defined", loc_);
			return; //TODO: Error, multi-definition
		}
		
		params[i] = v.release();
		cur = -1;
	}
	
	void addParam(unique_ptr<OmfgScript::TokenBase> v, Location loc_)
	{
		if(cur < 0 && !(flags & PosParamAfterNamed))
		{
			flags |= PosParamAfterNamed;
			error("Positional parameters not allowed after named parameters", loc_);
			return;
		}
		if(cur >= (int)params.size() && !(flags & MaxReachParam))
		{
			flags |= MaxReachParam;
			error(S_("Too many parameters. Maximum allowed is ") << params.size() << '.', loc_);
			return;
		}
		
		params[cur] = v.release();
		++cur;
	}
	
	void checkParams()
	{
		for(size_t i = 0; i < params.size(); ++i)
		{
			if(!params[i])
			{
				if(!paramDef->params[i].optional)
					error(S_("Parameter '") << paramDef->params[i].name << "' (number " << (i+1) << ") not defined and not optional");
				else
					params[i] = new TokenBase(loc);
			}
		}
	}
	
	void calcCRC(boost::crc_32_type& crc)
	{
		foreach(i, params)
		{
			(*i)->calcCRC(crc);
		}
	}
	
	void error(std::string const& msg)
	{
		error(msg, loc);
	}
	
	void error(std::string const& msg, Location loc_)
	{
		loc_.print(msg);
		flags |= Error;
	}
	
	int cur;
	int flags;
	ParamDef* paramDef;
	std::vector<OmfgScript::TokenBase*> params;
	Location loc;
};

struct ActionDef
{
	ActionDef(ParamDef* paramDef_, ActionFactory::CreateFunc create_, int requireMask_)
	: paramDef(paramDef_), create(create_), requireMask(requireMask_)
	{
	}
	
	~ActionDef()
	{
		delete paramDef;
	}
	
	ParamDef* paramDef;
	ActionFactory::CreateFunc create;
	int requireMask;
};

struct GameEventDef
{
	GameEventDef(std::string const& name_, ParamDef* paramDef_, int type_, int provideMask_)
	: name(name_), paramDef(paramDef_), type(type_), provideMask(provideMask_)
	{
	}
	
	~GameEventDef()
	{
		delete paramDef;
	}
	
	std::string name;
	ParamDef* paramDef;
	int type;
	int provideMask;
};

uint32_t TokenBase::toColor(int r, int g, int b)
{
	if(isDefault())
		return makecol(r, g, b);
	if(assertList())
	{
		std::list<TokenBase*> const& l = toList();
		if(l.size() != 3)
			loc.print("Expected three components in colour");
		let_(i, l.begin());
		r = (*i)->toInt(); ++i;
		g = (*i)->toInt(); ++i;
		b = (*i)->toInt(); ++i;
		return makecol(r, g, b);
	}
	loc.print("Expected colour");
	return makecol(r, g, b);
}

struct List : public TokenBase
{
	List(Location loc_)
	: TokenBase(loc_)
	{
	}
	
	void add(std::unique_ptr<TokenBase> el)
	{
		elements.push_back(el.release());
	}

	virtual std::list<TokenBase*> const& toList()
	{ return elements; }
	
	virtual std::ostream& output(std::ostream& s)
	{
		s << "[";
		bool first = true;
		foreach(i, elements)
		{
			if(!first)
				s << ", ";
			else
				first = false;
			(*i)->output(s);
		}
		s << "]";
		return s;
	}
	
	virtual Type::type type()
	{ return Type::List; }
	
	virtual void calcCRC(boost::crc_32_type& crc)
	{
		crc(0xE);
		foreach(i, elements)
		{
			(*i)->calcCRC(crc);
		}
	}
	
	~List()
	{
		foreach(i, elements)
			delete *i;
	}
	
	std::list<TokenBase *> elements;
};

Function::Function(Location loc_, std::string const& name_)
: TokenBase(loc_), name(name_)
{
}

Function::~Function()
{
	foreach(i, params)
		delete *i;
}

TokenBase* Function::operator[](size_t i) const
{
	if(i >= params.size())
	{
		loc.print(S_("Function has no parameter #") << (i+1));
		return &Parser::globalDefault;
	}
	return params[i];
}

void Function::calcCRC(boost::crc_32_type& crc)
{
	crc.process_bytes(name.data(), name.size());
	foreach(i, params)
	{
		(*i)->calcCRC(crc);
	}
}

struct Func : public Function
{
	Func(Location loc_, std::string const& name_)
	: Function(loc_, name_)
	{
	}
	
	void add(std::unique_ptr<TokenBase> el)
	{
		params.push_back(el.release());
	}
	
	virtual Function const* toFunction()
	{ return this; }
	
	virtual Type::type type()
	{ return Type::Func; }
};

struct BinOp : public TokenBase
{
	BinOp(Location loc_, TokenBase* a_, TokenBase* b_)
	: TokenBase(loc_), a(a_), b(b_)
	{
	}
	
	virtual Type::type type()
	{
		Type::type at = a->type();
		Type::type bt = b->type();
		if(at == bt)
		{
			return at;	
		}
		else
		if((at == Type::Double && bt == Type::Int)
		|| (at == Type::Int && bt == Type::Double))
			return Type::Double;
		return Type::Unknown;
	}

	~BinOp()
	{
		delete a;
		delete b;
	}
	
	TokenBase* a;
	TokenBase* b;
};

struct Add : public BinOp
{
	Add(Location loc_, TokenBase* a_, TokenBase* b_)
	: BinOp(loc_, a_, b_)
	{
	}
	
	virtual double toDouble()
	{
		return a->toDouble() + b->toDouble();
	}
	
	virtual int toInt()
	{
		if(a->type() == Type::Int && b->type() == Type::Int)
			return a->toInt() + b->toInt();
		return static_cast<int>(a->toDouble() + b->toDouble());
	}
	
	virtual void calcCRC(boost::crc_32_type& crc)
	{
		crc.process_byte(0xA);
		a->calcCRC(crc);
		b->calcCRC(crc);
	}
};

struct Sub : public BinOp
{
	Sub(Location loc_, TokenBase* a_, TokenBase* b_)
	: BinOp(loc_, a_, b_)
	{
	}
	
	virtual double toDouble()
	{
		return a->toDouble() - b->toDouble();
	}
	
	virtual int toInt()
	{
		if(a->type() == Type::Int && b->type() == Type::Int)
			return a->toInt() - b->toInt();
		return static_cast<int>(a->toDouble() - b->toDouble());
	}
	
	virtual void calcCRC(boost::crc_32_type& crc)
	{
		crc.process_byte(0xB);
		a->calcCRC(crc);
		b->calcCRC(crc);
	}
};

struct Mul : public BinOp
{
	Mul(Location loc_, TokenBase* a_, TokenBase* b_)
	: BinOp(loc_, a_, b_)
	{
	}
	
	virtual double toDouble()
	{
		return a->toDouble() * b->toDouble();
	}
	
	virtual int toInt()
	{
		if(a->type() == Type::Int && b->type() == Type::Int)
			return a->toInt() * b->toInt();
		return static_cast<int>(a->toDouble() * b->toDouble());
	}
	
	virtual void calcCRC(boost::crc_32_type& crc)
	{
		crc.process_byte(0xC);
		a->calcCRC(crc);
		b->calcCRC(crc);
	}
};

struct Div : public BinOp
{
	Div(Location loc_, TokenBase* a_, TokenBase* b_)
	: BinOp(loc_, a_, b_)
	{
	}
	
	virtual double toDouble()
	{
		return a->toDouble() / b->toDouble();
	}
	
	virtual int toInt()
	{
		if(a->type() == Type::Int && b->type() == Type::Int)
			return static_cast<int>(a->toInt() / b->toDouble());
		return static_cast<int>(a->toDouble() / b->toDouble());
	}
	
	virtual void calcCRC(boost::crc_32_type& crc)
	{
		crc.process_byte(0xD);
		a->calcCRC(crc);
		b->calcCRC(crc);
	}
};

}

#include "omfg_script_parser.h"

namespace OmfgScript
{
	
struct ActionFactoryImpl
{
	~ActionFactoryImpl()
	{
		foreach(i, actionDefs)
			delete i->second;
	}
	
	std::map<std::string, ActionDef*> actionDefs;
};

ParamProxy::ParamProxy(ParamDef* paramDef_)
: paramDef(paramDef_)
{
}

ParamProxy const& ParamProxy::operator()(std::string const& name, bool optional) const
{
	paramDef->addParam(name, optional);
	return *this;
}

ActionFactory::ActionFactory()
: Pimpl<ActionFactoryImpl>(new ActionFactoryImpl())
{
}
	
ActionFactory::~ActionFactory()
{
	delete pimpl;
}

ParamProxy ActionFactory::add(std::string const& name, ActionFactory::CreateFunc createFunc, int requireMask)
{
	ParamDef* paramDef = new ParamDef;
	pimpl->actionDefs[name] = new ActionDef(paramDef, createFunc, requireMask);
	return ParamProxy(paramDef);
}

ActionDef* ActionFactory::operator[](std::string const& name)
{
	let_(i, pimpl->actionDefs.find(name));
	if(i == pimpl->actionDefs.end())
		return 0;
	return i->second;
}

struct ParserImpl : public TGrammar<ParserImpl>
{
	struct GameEvent
	{
		GameEvent(GameEventDef* def_, Parameters* params_, std::vector< boost::shared_ptr<BaseAction> >& actions_)
		: def(def_), params(params_)
		{
			actions.swap(actions_);
		}
		
		~GameEvent()
		{
			delete params;
		}
		
		//std::string name;
		GameEventDef* def;
		Parameters* params;
		std::vector< boost::shared_ptr<BaseAction> > actions;
	};
	
	struct Property
	{
		Property(Location loc_, TokenBase* value_)
		: loc(loc_), touched(false), value(value_)
		{
		}
		
		~Property()
		{
			delete value;
		}
		
		Location   loc;
		bool       touched;
		TokenBase* value;
	};
	
	ParserImpl(std::istream& str_, ActionFactory& actionFactory_, std::string const& fileName_)
	: str(str_), fileName(fileName_), actionFactory(actionFactory_)
	{
		this->next();
	}
	
	~ParserImpl()
	{
		if(!error) // Don't bother doing this if there were errors
		{
			foreach(i, properties)
			{
				if(!i->second->touched)
					semanticWarning("Property '" + i->first + "' defined but not used", i->second->loc);
			}
		}
		
		foreach(i, properties)
			delete i->second;
		foreach(i, eventDef)
			delete i->second;
		foreach(i, events)
			delete *i;
	}
	
	size_t read(char* p, size_t s)
	{
		str.read(p, s);
		return (size_t)str.gcount();
	}
	
	void reportError(std::string const& error, Location loc)
	{
		loc.print(error);
	}
	
	Location getLoc()
	{
		return Location(fileName, line);
	}
	
	void property(std::string const& name, TokenBase* t, Location loc)
	{
		crc.process_bytes(name.data(), name.size());
		if(t) {
			t->calcCRC(crc);
			properties[name] = new Property(loc, t);
		}
	}
	
	TokenBase* getRawProperty(std::string const& name)
	{
		let_(i, properties.find(name));
		if(i != properties.end())
		{
			i->second->touched = true;
			return i->second->value;
		}
		return 0;
	}
	
	GameEventDef* getEventDef(std::string const& name)
	{
		crc.process_bytes(name.data(), name.size());
		let_(i, eventDef.find(name));
		if(i != eventDef.end())
		{
			return i->second;
		}
		
		int minDist = 1000000;
		std::string minEvent;
		foreach(i, eventDef)
		{
			int dist = levenshteinDistance(name, i->first);
			if(dist < minDist)
			{
				minDist = dist;
				minEvent = i->first;
			}
		}
		
		if(!minEvent.empty() && minDist <= (int)(name.size() / 2))
			semanticError("Unknown event '" + name + "'. Did you mean '" + minEvent + "'?");
		else
			semanticError("Unknown event '" + name + "'");
		return 0;
	}
	
	ActionDef* getAction(std::string const& name)
	{
		crc.process_bytes(name.data(), name.size());
		let_(i, actionFactory[name]);
		if(i)
		{
			return i;
		}
		
		
		int minDist = 1000000;
		std::string minAction;
		foreach(i, actionFactory.pimpl->actionDefs)
		{
			int dist = levenshteinDistance(name, i->first);
			if(dist < minDist)
			{
				minDist = dist;
				minAction = i->first;
			}
		}
		
		if(!minAction.empty() && minDist <= (int)(name.size() / 2))
			semanticError("Unknown action '" + name + "'. Did you mean '" + minAction + "'?");
		else
			semanticError("Unknown action '" + name + "'");

		return 0;
	}
	
	BaseAction* createAction(ActionDef* action, std::unique_ptr<Parameters> params)
	{
		params->calcCRC(crc);
		return action->create(params->params);
	}
	
	void addEvent(GameEventDef* event, std::unique_ptr<Parameters> params, std::vector< boost::shared_ptr<BaseAction> >& actions)
	{
		params->calcCRC(crc);
		events.push_back(new GameEvent(event, params.release(), actions));
	}
	
	std::istream& str;
	std::string fileName;
	std::map<std::string, Property*> properties;
	std::list<GameEvent*> events;
	boost::crc_32_type crc;
	
	//
	std::map<std::string, GameEventDef*> eventDef;
	ActionFactory& actionFactory;
};

struct GameEventData
{
	typedef std::list<ParserImpl::GameEvent*>::const_iterator Iter;
	GameEventData(Iter i_, Iter end_)
	: i(i_), end(end_)
	{
	}
	
	Iter i;
	Iter end;
};

TokenBase Parser::globalDefault(Location("<nowhere>", 0));

#define self (static_cast<GameEventData *>(data))
Parser::GameEventIter::~GameEventIter()
{
	delete self;
}
	
Parser::GameEventIter& Parser::GameEventIter::operator++()
{
	++self->i;
	return *this;
}

Parser::GameEventIter::operator bool()
{
	return self->i != self->end;
}

int Parser::GameEventIter::type()
{
	return (*self->i)->def->type;
}
	
std::vector<TokenBase*> const& Parser::GameEventIter::params()
{
	return (*self->i)->params->params;
}

std::vector< boost::shared_ptr<BaseAction> >& Parser::GameEventIter::actions()
{
	return (*self->i)->actions;
}

Parser::GameEventIter::GameEventIter(Parser& parser)
: data(new GameEventData(parser.pimpl->events.begin(), parser.pimpl->events.end()))
{
	
}

#undef self

Parser::Parser(std::istream& str, ActionFactory& actionFactory, std::string const& fileName)
: Pimpl<ParserImpl>(new ParserImpl(str, actionFactory, fileName))
{
}
	
Parser::~Parser()
{
	delete pimpl;
}

ParamProxy Parser::addEvent(std::string const& name, int type, int provideMask)
{
	ParamDef* paramDef = new ParamDef;
	pimpl->eventDef[name] = new GameEventDef(name, paramDef, type, provideMask);
	return ParamProxy(paramDef);
}

double Parser::getDouble(std::string const& name, double def)
{
	return getProperty(name)->toDouble(def);
}

std::string const& Parser::getString(std::string const& name, std::string const& def)
{
	return getProperty(name)->toString(def);
}

std::list<TokenBase*> const& Parser::getList(std::string const& name, std::list<TokenBase*> const& def)
{
	return getProperty(name)->toList(def);
}

Function const* Parser::getFunction(std::string const& name)
{
	TokenBase* v = getProperty(name);
	if(v->isFunction())
		return v->toFunction();
	return 0;
}

Function const* Parser::getDeprFunction(std::string const& name)
{
	TokenBase* v = getDeprProperty(name);
	if(v->isFunction())
		return v->toFunction();
	return 0;
}

int Parser::getInt(std::string const& name, int def)
{
	return getProperty(name)->toInt(def);
}

bool Parser::getBool(std::string const& name, bool def)
{
	return getProperty(name)->toBool(def);
}

TokenBase* Parser::getProperty(std::string const& a)
{
	if(TokenBase* p = pimpl->getRawProperty(a)) return p;
	return &globalDefault;
}

TokenBase* Parser::getRawProperty(std::string const& a)
{
	return pimpl->getRawProperty(a);
}

TokenBase* Parser::getDeprProperty(std::string const& a)
{
	if(TokenBase* p = pimpl->getRawProperty(a))
	{
		p->loc.print("Property '" + a + "' is deprecated");
		return p;
	}
	return &globalDefault;
}

TokenBase* Parser::getProperty(std::string const& a, std::string const& b)
{
	if(TokenBase* p = pimpl->getRawProperty(a)) return p;
	else if(TokenBase* p = pimpl->getRawProperty(b)) return p;
	return &globalDefault;
}

bool Parser::run()
{
	pimpl->rule_lines();
	return pimpl->full();
}

bool Parser::incomplete()
{
	return pimpl->cur != 0;
}

void Parser::error(std::string const& msg)
{
	pimpl->semanticError(msg);
}

boost::crc_32_type::value_type Parser::getCRC()
{
	return pimpl->crc.checksum();
}

void Parser::crcProcessByte(unsigned char byte)
{
	pimpl->crc.process_byte(byte);
}

}

