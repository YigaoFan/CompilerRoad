export module Context;

import std;
import SymbolTable;

using std::unique_ptr;

struct Context
{
	Scope* CurrentScope = nullptr; // only some special node can own the scope's ownership, like function definition, compound statement, etc.
};

export
{
	struct Context;
}