export module ConflictResolver;

import std;
import Base;
import Lexer;
import Parser;
import C11Spec;
import Context;

using std::vector;
using std::map;
using std::pair;
using std::expected;
using std::unexpected;
using std::stack;
using std::set;
using std::format;

class C11ConflictResolver
{
private:
	template <template <typename> class Stream, typename Token>
    using ResolveFunc = auto (C11ConflictResolver::*)(stack<SyntaxTreeNode<Token, Context>*>&, vector<SimpleRightSide> const&, Stream<Token>&) const -> expected<int, ParseFailResult>;
    template <template <typename> class Stream, typename Token>
    static map<pair<String, TokType>, ResolveFunc<Stream, Token>> const resolveMap;

    // find the index of the option whose right-side contains the given symbol
    static auto FindOptionIndex(vector<SimpleRightSide> const& options, String const& symbol) -> expected<int, ParseFailResult>
    {
        for (int i = 0; i < static_cast<int>(options.size()); ++i)
        {
            for (auto const& s : options[i])
            {
                if (s == symbol)
                {
                    return i;
                }
            }
        }
		return unexpected(ParseFailResult{ .Message = format("no option contains symbol: {}", symbol) });
    }

    // find the index of the option whose right-side not contain the given symbol
    static auto FindNotContainOptionIndex(vector<SimpleRightSide> const& options, String const& symbol) -> expected<int, ParseFailResult>
    {
        if (options.size() != 2)
        {
            return unexpected(ParseFailResult{ .Message = format("FindNotContainOptionIndex only support 2 options: {}", symbol) });
        }

		if (auto optIdx = FindOptionIndex(options, symbol); optIdx.has_value())
		{
			// return the another item
			switch (optIdx.value())
			{
			case 0:
				return 1;
			case 1:
				return 0;
			}
		}
        return unexpected(ParseFailResult{ .Message = format("not found the {} for chosing another option", symbol) });
    }

    // find the index of the empty option (right-side is empty)
    static auto FindEmptyOptionIndex(vector<SimpleRightSide> const& options) -> expected<int, ParseFailResult>
    {
        for (int i = 0; i < static_cast<int>(options.size()); ++i)
        {
            if (options[i].empty())
            {
                return i;
            }
        }
		return unexpected(ParseFailResult{ .Message = "no empty option found" });
    }

    // find the first non-null Context from the working nodes stack (parent nodes)
    template <typename Token>
    static auto FindContextFromWorkingNodes(stack<SyntaxTreeNode<Token, Context>*> workingNodes) -> Context const*
    {
        // workingNodes.top() is the current node (being expanded, its Context may not be set yet)
        // nodes below it are ancestors whose Context may already be populated
        while (!workingNodes.empty())
        {
            auto node = workingNodes.top();
            workingNodes.pop();
            if (node != nullptr)
            {
                return &node->Result;
            }
        }
        return nullptr;
    }

    // check if an identifier is a typedef-name in the given context's scope
    static auto IsTypedefName(Context const* context, std::string const& name) -> bool
    {
        String nameStr{ name };
        if (context == nullptr or context->CurrentScope == nullptr or context->CurrentScope->Contains(nameStr))
        {
            return false;
        }
        
        auto const& symbol = context->CurrentScope->Get(nameStr);
		return symbol.Type.Tag == TypeInfo::Kind::TypedefName;
    }

public:
    auto Resolvable(String nontermin, int tokType) const -> bool
    {
        return resolveMap<VectorStream, Token<TokType>>.contains({ nontermin, static_cast<TokType>(tokType) });
    }

    // TODO add to concept, not using now
    auto ResolvedConflicts() -> set<pair<String, TokType>> const&
    {
		static auto resolvedConflicts = std::ranges::views::keys(resolveMap<VectorStream, Token<TokType>>) | std::ranges::to<set>();
        return resolvedConflicts;
    }

    template <template <typename> class ActualStream, typename Token>
        requires Stream<ActualStream, Token>
    auto Resolve(stack<SyntaxTreeNode<Token, Context>*> workingNodes, String nontermin, TokType tokType, vector<SimpleRightSide> const& options, ActualStream<Token>& stream) const -> expected<int, ParseFailResult>
    {
        auto it = resolveMap<ActualStream, Token>.find({ nontermin, tokType });
        if (it != resolveMap<ActualStream, Token>.end())
        {
            return (this->*it->second)(workingNodes, options, stream);
        }
        return unexpected(ParseFailResult{ .Message = format("unhandled conflict: {} with {}", nontermin, tokType) });
    }

    // conflict: declaration-specifiers_com_1 + _Atomic
    // options: {"type-specifier", ...} vs {"type-qualifier", ...} vs others
    // If _Atomic is followed by "(", it's atomic-type-specifier (type-specifier), else type-qualifier
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveTypeSpecifierAndTypeQualifier(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (not stream.MoveNext())
        {
            return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve type-specifier and type-qualifier conflict" });
        }

        if (stream.Current().Type == TokType::Punctuator_LeftParen)
        {
            return FindOptionIndex(options, "type-specifier");
        }
        return FindOptionIndex(options, "type-qualifier");
    }

    // conflict: declaration-specifiers_op_2 + Identifier
    // options: {"declaration-specifiers"} vs {} (empty)
    // If the identifier is a typedef-name, continue with declaration-specifiers (it's a type specifier).
    // Otherwise, it's the start of init-declarator (empty option) — a non-typedef Identifier
    // followed by `(` or `[` is a function/array declarator name, not a type specifier.
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDeclarationSpecifiersAndInitDeclaratorList(stack<SyntaxTreeNode<Tok, Context>*>& workingNodes, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        auto context = FindContextFromWorkingNodes(workingNodes);
        if (IsTypedefName(context, stream.Current().Value))
        {
            return FindOptionIndex(options, "declaration-specifiers");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: bracket-suffix + *
    // Shared by direct-abstract-declarator-[-suffix, direct-abstract-declarator'-[-suffix, direct-declarator'-[-suffix
    // [*] is VLA; *expr is dereference in assignment-expression. Disambiguate by checking if "]" follows.
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveBracketSuffixStar(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (not stream.MoveNext())
        {
            return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve bracket suffix (star)" });
        }
        if (stream.Current().Type == TokType::Punctuator_RightBracket)
        {
            return FindOptionIndex(options, "*");
        }
        return FindNotContainOptionIndex(options, "*");
    }

    // conflict: direct-abstract-declarator-[-suffix + type-qualifier
    // options: normal-array vs typequal-static-array
    // Look ahead through type-qualifier-list: if "static" → option with "static", if "]" → normal-array option
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectAbstractDeclaratorBracketSuffixTypeQualifier(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        while (stream.MoveNext())
        {
            auto t = stream.Current();
            if (t.Type == TokType::Keyword_Static)
            {
                return FindOptionIndex(options, "static");
            }
            if (t.Type == TokType::Punctuator_RightBracket)
            {
                return FindOptionIndex(options, "direct-abstract-declarator_op_22");
            }
            if (t.Type == TokType::Punctuator_Comma or t.Type == TokType::Punctuator_Semicolon or t.Type == TokType::Punctuator_RightParen)
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-abstract-declarator bracket suffix" });
            }
            // else: another type-qualifier, continue loop
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-abstract-declarator bracket suffix" });
    }

    // conflict: direct-abstract-declarator'-[-suffix + type-qualifier
    // Same pattern as direct-abstract-declarator-[-suffix type-qualifier
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectAbstractDeclaratorRrBracketSuffixTypeQualifier(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        while (stream.MoveNext())
        {
            auto t = stream.Current();
            if (t.Type == TokType::Keyword_Static)
            {
                return FindOptionIndex(options, "static");
            }
            if (t.Type == TokType::Punctuator_RightBracket)
            {
                return FindOptionIndex(options, "direct-abstract-declarator'");
            }
            if (t.Type == TokType::Punctuator_Comma or t.Type == TokType::Punctuator_Semicolon or t.Type == TokType::Punctuator_RightParen)
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-abstract-declarator' bracket suffix" });
            }
            // else: another type-qualifier, continue loop
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-abstract-declarator' bracket suffix" });
    }

    // conflict: direct-declarator'-(-suffix + Identifier
    // options: {"parameter-type-list", ...} vs {"direct-declarator'", ...}
    // If the identifier is a typedef-name, it starts a parameter-type-list (function declaration).
    // Otherwise, it's a direct-declarator suffix (e.g., function call or array subscript).
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectDeclaratorRrParenSuffix(stack<SyntaxTreeNode<Tok, Context>*>& workingNodes, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        auto context = FindContextFromWorkingNodes(workingNodes);
        if (IsTypedefName(context, stream.Current().Value))
        {
            return FindOptionIndex(options, "parameter-type-list");
        }
        return FindOptionIndex(options, "direct-declarator'");
    }

    // conflict: direct-declarator'-[-suffix + type-qualifier
    // options: normal-array vs typequal-static-array
    // Look ahead through type-qualifier-list: if "static" → option with "static", if "]" → normal-array option
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectDeclaratorRrBracketSuffixTypeQualifier(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        while (stream.MoveNext())
        {
            auto t = stream.Current();
            if (t.Type == TokType::Keyword_Static)
            {
                return FindOptionIndex(options, "static");
            }
            if (t.Type == TokType::Punctuator_RightBracket)
            {
                return FindOptionIndex(options, "direct-declarator_op_12");
            }
            if (t.Type == TokType::Punctuator_Comma or t.Type == TokType::Punctuator_Semicolon or t.Type == TokType::Punctuator_RightParen)
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-declarator' bracket suffix" });
            }
            // else: another type-qualifier, continue loop
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-declarator' bracket suffix" });
    }

    // conflict: enum-specifier-enum-suffix + Identifier
    // options: {enum-specifier_op_8 { enumerator-list ...} vs {Identifier}
    // Look ahead: if next is "{", enum definition, else enum reference
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveEnumSpecifierEnumSuffix(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve enum-specifier-enum-suffix" });
        }
        if (stream.Current().Type == TokType::Punctuator_LeftBrace)
        {
            return FindOptionIndex(options, "{"); // enum definition with { }
        }
        return FindOptionIndex(options, "Identifier"); // enum reference, just Identifier
    }

    // conflict: enumerator-list_op_12 + ","
    // options: {enumerator-list'} vs {} (empty)
    // Look ahead: if next is Identifier, continue, else trailing comma
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveEnumeratorListComma(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return FindEmptyOptionIndex(options);
        }
        if (stream.Current().Type == TokType::Identifier)
        {
            return FindOptionIndex(options, "enumerator-list'");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: enumerator-list' + ","
    // Same pattern as enumerator-list_op_12
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveEnumeratorListRrComma(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return FindEmptyOptionIndex(options);
        }
        if (stream.Current().Type == TokType::Identifier)
        {
            return FindOptionIndex(options, "enumerator-list'");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: initializer-list' + ","
	// options: {...initializer-list'} vs {} (empty)
    // Look ahead: if next can start an initializer, continue, else trailing comma
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveInitializerListRrComma(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (not stream.MoveNext())
        {
            return FindEmptyOptionIndex(options);
        }
        auto t = stream.Current();
        if (t.Type == TokType::Punctuator_LeftBracket or t.Type == TokType::Punctuator_LeftBrace or t.Type == TokType::Punctuator_LeftParen or t.Type == TokType::Punctuator_Star or t.Type == TokType::Identifier or
            t.Type == TokType::IntegerConstant or t.Type == TokType::FloatingConstant or
            t.Type == TokType::CharacterConstant or t.Type == TokType::StringLiteral or
            t.Type == TokType::Punctuator_Exclamation or t.Type == TokType::Punctuator_Tilde or t.Type == TokType::Punctuator_Ampersand or t.Type == TokType::Punctuator_Minus or t.Type == TokType::Punctuator_Plus)
        {
            return FindOptionIndex(options, "initializer-list'");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: parameter-list_op_24 + ","
    // options: {parameter-list'} vs {} (empty)
    // Look ahead: if next can start a parameter-declaration, continue, else trailing comma
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveParameterListComma(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return FindEmptyOptionIndex(options);
        }
        auto t = stream.Current();
        if (t.Type == TokType::Identifier or
            t.Type == TokType::Keyword_Typedef or t.Type == TokType::Keyword_Extern or t.Type == TokType::Keyword_Static or
            t.Type == TokType::Keyword__Thread_Local or t.Type == TokType::Keyword_Auto or t.Type == TokType::Keyword_Register or
            t.Type == TokType::Keyword_Void or t.Type == TokType::Keyword_Char or t.Type == TokType::Keyword_Short or t.Type == TokType::Keyword_Int or
            t.Type == TokType::Keyword_Long or t.Type == TokType::Keyword_Float or t.Type == TokType::Keyword_Double or
            t.Type == TokType::Keyword_Signed or t.Type == TokType::Keyword_Unsigned or t.Type == TokType::Keyword__Bool or t.Type == TokType::Keyword__Complex or
            t.Type == TokType::Keyword__Atomic or t.Type == TokType::Keyword_Struct or t.Type == TokType::Keyword_Union or t.Type == TokType::Keyword_Enum or
            t.Type == TokType::Keyword_Const or t.Type == TokType::Keyword_Restrict or t.Type == TokType::Keyword_Volatile or
            t.Type == TokType::Keyword_Inline or t.Type == TokType::Keyword__Noreturn or t.Type == TokType::Keyword__Alignas)
        {
            return FindOptionIndex(options, "parameter-list'");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: parameter-list' + ","
    // Same pattern as parameter-list_op_24
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveParameterListRrComma(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return FindEmptyOptionIndex(options);
        }
        auto t = stream.Current();
        if (t.Type == TokType::Identifier or
            t.Type == TokType::Keyword_Typedef or t.Type == TokType::Keyword_Extern or t.Type == TokType::Keyword_Static or
            t.Type == TokType::Keyword__Thread_Local or t.Type == TokType::Keyword_Auto or t.Type == TokType::Keyword_Register or
            t.Type == TokType::Keyword_Void or t.Type == TokType::Keyword_Char or t.Type == TokType::Keyword_Short or t.Type == TokType::Keyword_Int or
            t.Type == TokType::Keyword_Long or t.Type == TokType::Keyword_Float or t.Type == TokType::Keyword_Double or
            t.Type == TokType::Keyword_Signed or t.Type == TokType::Keyword_Unsigned or t.Type == TokType::Keyword__Bool or t.Type == TokType::Keyword__Complex or
            t.Type == TokType::Keyword__Atomic or t.Type == TokType::Keyword_Struct or t.Type == TokType::Keyword_Union or t.Type == TokType::Keyword_Enum or
            t.Type == TokType::Keyword_Const or t.Type == TokType::Keyword_Restrict or t.Type == TokType::Keyword_Volatile or
            t.Type == TokType::Keyword_Inline or t.Type == TokType::Keyword__Noreturn or t.Type == TokType::Keyword__Alignas)
        {
            return FindOptionIndex(options, "parameter-list'");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: specifier-qualifier-list + _Atomic
    // options: {type-specifier specifier-qualifier-list_op_5} vs {type-qualifier specifier-qualifier-list_op_6}
    // If _Atomic is followed by "(", it's atomic-type-specifier (type-specifier), else type-qualifier
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveSpecifierQualifierListAtomic(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve specifier-qualifier-list _Atomic conflict" });
        }
        if (stream.Current().Type == TokType::Punctuator_LeftParen)
        {
            return FindOptionIndex(options, "type-specifier");
        }
        return FindOptionIndex(options, "type-qualifier");
    }

    // conflict: parameter-declaration-declaration-specifiers-suffix + (/ *
    // options: {declarator} vs {abstract-declarator}
    // For *: look ahead — if followed by a non-typedef Identifier, it's a pointer declarator (e.g. int *p),
    //        otherwise it's an abstract pointer declarator (e.g. int * or int *(*)(int)).
    // For (: look ahead — if the next token after ( is )/type-keyword/typedef-name, it's a function
    //        parameter list (abstract-declarator); if it's Identifier/*/(/[ it's a grouping/parenthesized declarator.
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveParameterDeclarationSuffix(stack<SyntaxTreeNode<Tok, Context>*>& workingNodes, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (options.size() != 2)
        {
            return unexpected(ParseFailResult{ .Message = "only adapt 2 options of conflicts parameter-declaration-declaration-specifiers-suffix + (/ *" });
        }
        auto context = FindContextFromWorkingNodes(workingNodes);

        if (stream.Current().Type == TokType::Punctuator_Star)
        {
            // Look ahead: is there a non-typedef Identifier after the pointer?
            // If so, it's a named pointer declarator (e.g. int *p).
            // Otherwise, abstract (e.g. int * or int *(*)(int)).
            if (not stream.MoveNext())
            {
                return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve parameter-declaration * conflict" });
            }
			if (stream.Current().Type == TokType::Identifier and not IsTypedefName(context, stream.Current().Value))
            {
                return FindOptionIndex(options, "declarator");
            }
            else
            {
                return FindNotContainOptionIndex(options, "declarator");
            }
        }

        if (stream.Current().Type == TokType::Punctuator_LeftParen)
        {
            if (not stream.MoveNext())
            {
                return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve parameter-declaration ( conflict" });
            }
            auto next = stream.Current();
            // () or (void) or (int, ...) — function parameter list → abstract-declarator
            if (next.Type == TokType::Punctuator_RightParen or next.Type == TokType::Keyword_Void or
                next.Type == TokType::Keyword_Char or next.Type == TokType::Keyword_Short or next.Type == TokType::Keyword_Int or
                next.Type == TokType::Keyword_Long or next.Type == TokType::Keyword_Float or next.Type == TokType::Keyword_Double or
                next.Type == TokType::Keyword_Signed or next.Type == TokType::Keyword_Unsigned or
                next.Type == TokType::Keyword__Bool or next.Type == TokType::Keyword__Complex or
                next.Type == TokType::Keyword_Struct or next.Type == TokType::Keyword_Union or next.Type == TokType::Keyword_Enum or
                next.Type == TokType::Keyword_Const or next.Type == TokType::Keyword_Restrict or next.Type == TokType::Keyword_Volatile or
                next.Type == TokType::Keyword__Atomic or next.Type == TokType::Keyword_Inline or next.Type == TokType::Keyword__Noreturn or
                next.Type == TokType::Punctuator_Ellipsis or
                (next.Type == TokType::Identifier and IsTypedefName(context, next.Value)))
            {
                return FindNotContainOptionIndex(options, "declarator");
            }
            // (*) — unnamed function pointer parameter → abstract-declarator
            if (next.Type == TokType::Punctuator_Star)
            {
                if (not stream.MoveNext())
                {
                    return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve parameter-declaration (*" });
                }
                if (stream.Current().Type == TokType::Punctuator_RightParen)
                {
                    return FindNotContainOptionIndex(options, "declarator");
                }
                // (*p) or (*p)(int) — named pointer declarator
                return FindOptionIndex(options, "declarator");
            }
            // Identifier (non-typedef), (, [ — it's a parenthesized declarator (e.g. int (x))
            return FindOptionIndex(options, "declarator");
        }

        // Should not be reached (only * and ( cause this conflict)
        return FindOptionIndex(options, "declarator");
    }

    // conflict: specifier-qualifier-list_op_5/op_6 + Identifier
    // options: {specifier-qualifier-list} vs {} (empty)
    // If identifier is a typedef-name, continue with specifier-qualifier-list, else empty (struct-declarator)
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveSpecifierQualifierListIdentifier(stack<SyntaxTreeNode<Tok, Context>*>& workingNodes, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        auto context = FindContextFromWorkingNodes(workingNodes);
        if (IsTypedefName(context, stream.Current().Value))
        {
            // typedef-name is a type-specifier, continue specifier-qualifier-list
            return FindOptionIndex(options, "specifier-qualifier-list");
        }
        // Not a typedef-name, it's a struct-declarator (empty option)
        return FindEmptyOptionIndex(options);
    }

    // conflict: struct-declarator + Identifier/(/ *
    // options: {declarator} vs {struct-declarator_op_9 : constant-expression} (bit-field)
    // For Identifier: lookahead — if followed by ":", it's a bit-field (e.g. x : 3), else declarator (e.g. x).
    // For ( and *: they start a complex declarator. Bit-fields with ( or * are vanishingly rare,
    // so default to declarator. For *, skip pointer tokens and check for ":" after them.
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveStructDeclarator(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (stream.Current().Type == TokType::Punctuator_Star)
        {
            // Skip pointer indirections: *(*...
            while (stream.Current().Type == TokType::Punctuator_Star)
            {
                if (not stream.MoveNext())
                {
                    return FindOptionIndex(options, "declarator");
                }
            }
            // After pointers, check for ":" — if present, it's a bit-field with pointer declarator
            if (stream.Current().Type == TokType::Punctuator_Colon)
            {
                return FindOptionIndex(options, ":");
            }
            return FindOptionIndex(options, "declarator");
        }

        if (stream.Current().Type == TokType::Punctuator_LeftParen)
        {
            // "(" can start either a grouped declarator (e.g. (x)) or a bit-field with grouped declarator.
            // Matching balanced parentheses to find ":" is complex; default to declarator.
            // Bit-fields with grouped declarators are extremely rare in practice.
            return FindOptionIndex(options, "declarator");
        }

        // Identifier (guaranteed not a typedef-name by specifier-qualifier-list phase)
        // Look ahead: ":" means bit-field (e.g. x : 3), otherwise it's a member name (e.g. x ;)
        if (stream.MoveNext() and stream.Current().Type == TokType::Punctuator_Colon)
        {
            return FindOptionIndex(options, ":");
        }
        return FindOptionIndex(options, "declarator");
    }

    // conflict: alignment-specifier-_Alignas-suffix-(-suffix + Identifier
    // options: {type-name )} vs {constant-expression )}
    // If the identifier is a typedef-name, it's a type-name (e.g. _Alignas(MyType)).
    // Otherwise, it's a constant-expression (e.g. _Alignas(x) where x is a variable).
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveAlignmentSpecifierTypeNameAndConstantExpression(stack<SyntaxTreeNode<Tok, Context>*>& workingNodes, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        auto context = FindContextFromWorkingNodes(workingNodes);
        if (IsTypedefName(context, stream.Current().Value))
        {
            // Identifier is a typedef-name, parse as type-name
            return FindOptionIndex(options, "type-name");
        }
        // Not a typedef-name, parse as constant-expression
        return FindOptionIndex(options, "constant-expression");
    }
};

template <template <typename> class Stream, typename Token>
map<pair<String, TokType>, C11ConflictResolver::ResolveFunc<Stream, Token>> const C11ConflictResolver::resolveMap =
{
    { { "declaration-specifiers_com_1",                         TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveTypeSpecifierAndTypeQualifier<Stream, Token> },
    { { "declaration-specifiers_op_2",                          TokType::Identifier },            &C11ConflictResolver::ResolveDeclarationSpecifiersAndInitDeclaratorList<Stream, Token> },
    { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword_Const },         &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword_Restrict },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword_Volatile },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator-[-suffix",                  TokType::Punctuator_Star },       &C11ConflictResolver::ResolveBracketSuffixStar<Stream, Token> },
    { { "direct-abstract-declarator'-[-suffix",                 TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator'-[-suffix",                 TokType::Keyword_Const },         &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator'-[-suffix",                 TokType::Keyword_Restrict },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator'-[-suffix",                 TokType::Keyword_Volatile },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-abstract-declarator'-[-suffix",                 TokType::Punctuator_Star },       &C11ConflictResolver::ResolveBracketSuffixStar<Stream, Token> },
    { { "direct-declarator'-(-suffix",                          TokType::Identifier },            &C11ConflictResolver::ResolveDirectDeclaratorRrParenSuffix<Stream, Token> },
    { { "direct-declarator'-[-suffix",                          TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-declarator'-[-suffix",                          TokType::Keyword_Const },         &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-declarator'-[-suffix",                          TokType::Keyword_Restrict },      &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-declarator'-[-suffix",                          TokType::Keyword_Volatile },      &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffixTypeQualifier<Stream, Token> },
    { { "direct-declarator'-[-suffix",                          TokType::Punctuator_Star },       &C11ConflictResolver::ResolveBracketSuffixStar<Stream, Token> },
    { { "enum-specifier-enum-suffix",                           TokType::Identifier },            &C11ConflictResolver::ResolveEnumSpecifierEnumSuffix<Stream, Token> },
    { { "enumerator-list_op_12",                                TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveEnumeratorListComma<Stream, Token> },
    { { "enumerator-list'",                                     TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveEnumeratorListRrComma<Stream, Token> },
    { { "initializer-list'",                                    TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveInitializerListRrComma<Stream, Token> },
    { { "parameter-declaration",                                TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveTypeSpecifierAndTypeQualifier<Stream, Token> },
    { { "parameter-declaration-declaration-specifiers-suffix",  TokType::Punctuator_LeftParen },  &C11ConflictResolver::ResolveParameterDeclarationSuffix<Stream, Token> },
    { { "parameter-declaration-declaration-specifiers-suffix",  TokType::Punctuator_Star },       &C11ConflictResolver::ResolveParameterDeclarationSuffix<Stream, Token> },
    { { "parameter-list",                                       TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveTypeSpecifierAndTypeQualifier<Stream, Token> },
    { { "parameter-type-list",                                  TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveTypeSpecifierAndTypeQualifier<Stream, Token> },
    { { "parameter-list_op_24",                                 TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveParameterListComma<Stream, Token> },
    { { "parameter-list'",                                      TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveParameterListRrComma<Stream, Token> },
    { { "specifier-qualifier-list",                             TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveSpecifierQualifierListAtomic<Stream, Token> },
    { { "specifier-qualifier-list_op_5",                        TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveSpecifierQualifierListAtomic<Stream, Token> },
    { { "specifier-qualifier-list_op_5",                        TokType::Identifier },            &C11ConflictResolver::ResolveSpecifierQualifierListIdentifier<Stream, Token> },
    { { "specifier-qualifier-list_op_6",                        TokType::Identifier },            &C11ConflictResolver::ResolveSpecifierQualifierListIdentifier<Stream, Token> },
    { { "struct-declarator",                                    TokType::Punctuator_LeftParen },  &C11ConflictResolver::ResolveStructDeclarator<Stream, Token> },
    { { "struct-declarator",                                    TokType::Punctuator_Star },       &C11ConflictResolver::ResolveStructDeclarator<Stream, Token> },
    { { "struct-declarator",                                    TokType::Identifier },            &C11ConflictResolver::ResolveStructDeclarator<Stream, Token> },
    { { "alignment-specifier-_Alignas-suffix-(-suffix",         TokType::Identifier },            &C11ConflictResolver::ResolveAlignmentSpecifierTypeNameAndConstantExpression<Stream, Token> },
};

export
{
    class C11ConflictResolver;
}
