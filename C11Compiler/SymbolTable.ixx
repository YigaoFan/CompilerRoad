export module SymbolTable;

import std;
import Base;

using std::map;
using std::unique_ptr;
using std::vector;
using std::format;

struct TypeInfo
{
	enum class Kind // is it complete?
	{
		Primitive,
		Pointer,
		Array,
		Struct,
		Union,
		Enum,
		Function,
		TypedefName,
	};
	Kind Tag;
};

struct Symbol
{
	String Name;
	TypeInfo Type;
};

struct Scope
{
	map<String, Symbol> Symbols;
	Scope* ParentScope = nullptr;
	vector<unique_ptr<Scope>> Children;

	bool Contains(String const& name) const
	{
		return Symbols.contains(name) or (ParentScope != nullptr and ParentScope->Contains(name));
	}

	Symbol& Get(String const& name)
	{
		if (Symbols.contains(name))
		{
			return Symbols.at(name);
		}
		else if (ParentScope != nullptr)
		{
			return ParentScope->Get(name);
		}
		else
		{
			throw std::out_of_range(format("Symbol not found: {}", name));
		}
	}
};

// write a symbol visitor to construct the symbol table TODO

export
{
	struct Scope;
	struct TypeInfo;
}