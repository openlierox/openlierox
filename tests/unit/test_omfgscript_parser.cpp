/*
 *  Unit tests for the gusanos omfgscript parser (omfg_script.h).
 *  A gusanos mod is downloaded from the server and is thus untrusted,
 *  so a pathologically nested script must abort the parse
 *  rather than overflow the stack.
 */

#include "unittest.h"
#include "gusanos/omfgscript/omfg_script.h"

#include <sstream>
#include <string>

using namespace OmfgScript;

// A deeply nested expression must abort the parse, not crash.
// Before the recursion-depth guard each '(' recursed one level,
// and enough of them overflowed the stack (SIGSEGV);
// now the guard aborts via fatalError and run() returns false.
void test_OmfgScriptDeepExpression() {
	std::string src = "foo = ";
	src += std::string(100000, '(');   // 100k unclosed parens
	ActionFactory af;
	std::istringstream ss(src);
	Parser parser(ss, af, "test_deep_expr");
	CHECK(!parser.run());              // returns (no crash) and fails to parse
}

// The same for deeply nested property blocks (rule_prop recurses on '{').
void test_OmfgScriptDeepPropertyBlocks() {
	std::string src;
	for(int i = 0; i < 100000; ++i)
		src += "a{";                   // a{a{a{...
	ActionFactory af;
	std::istringstream ss(src);
	Parser parser(ss, af, "test_deep_props");
	CHECK(!parser.run());
}

// A normal, shallow script still parses successfully:
// the depth guard must not reject legitimate input.
void test_OmfgScriptShallowOk() {
	std::string src = "foo = (1 + 2) * 3\n";
	ActionFactory af;
	std::istringstream ss(src);
	Parser parser(ss, af, "test_shallow");
	CHECK(parser.run());
}

// An out-of-range integer literal in a mod must not crash the parser.
// lexical_cast<int> throws bad_lexical_cast, which used to escape uncaught
// out of run(); now it degrades to a semantic error, so run() just returns.
// (Reaching the end of the test without aborting is the assertion.)
void test_OmfgScriptHugeIntegerLiteral() {
	std::string src = "foo = 99999999999\n";
	ActionFactory af;
	std::istringstream ss(src);
	Parser parser(ss, af, "test_huge_int");
	parser.run();
	CHECK(true);
}

// The same throw can happen while the parser constructor reads the first token,
// before run() is ever called.
// A file that starts with an out-of-range number must not crash during construction.
void test_OmfgScriptHugeNumberFirstToken() {
	std::string src = "999999999999999999999999999999\n";
	ActionFactory af;
	std::istringstream ss(src);
	Parser parser(ss, af, "test_huge_first");
	parser.run();
	CHECK(true);
}
