export module ConflictResolver;

import std;
import Base;
import Parser;
import C11Spec;
import Context;

using std::vector;
using std::set;
using std::pair;
using std::expected;
using std::unexpected;
using std::stack;

class C11ConflictResolver
{
private:
    set<pair<String, TokType>> conflicts =
    {
        { "declaration-specifiers_com_1", TokType::Keyword__Atomic },
        { "declaration-specifiers_op_2", TokType::Identifier },
        { "direct-abstract-declarator-[-suffix", TokType::Keyword__Atomic },
        { "direct-abstract-declarator-[-suffix", TokType::Keyword_Const },
        { "direct-abstract-declarator-[-suffix", TokType::Keyword_Restrict },
        { "direct-abstract-declarator-[-suffix", TokType::Keyword_Volatile },
        { "direct-abstract-declarator_rr-[-suffix", TokType::Keyword__Atomic },
        { "direct-abstract-declarator_rr-[-suffix", TokType::Keyword_Const },
        { "direct-abstract-declarator_rr-[-suffix", TokType::Keyword_Restrict },
        { "direct-abstract-declarator_rr-[-suffix", TokType::Keyword_Volatile },
        { "direct-declarator_rr-(-suffix", TokType::Identifier },
        { "direct-declarator_rr-[-suffix", TokType::Keyword__Atomic },
        { "direct-declarator_rr-[-suffix", TokType::Keyword_Const },
        { "direct-declarator_rr-[-suffix", TokType::Keyword_Restrict },
        { "direct-declarator_rr-[-suffix", TokType::Keyword_Volatile },
        { "enum-specifier-enum-suffix", TokType::Identifier },
        { "enumerator-list_op_12", TokType::Punctuator_Comma },
        { "enumerator-list_rr_op_13", TokType::Punctuator_Comma },
        { "init-declarator-list", TokType::Punctuator_LeftParen },
        { "init-declarator-list", TokType::Punctuator_Star },
        { "init-declarator-list", TokType::Identifier },
        { "initializer-list_op_40", TokType::Punctuator_Comma },
        { "initializer-list_rr_op_42", TokType::Punctuator_Comma },
        { "parameter-declaration-declaration-specifiers-suffix", TokType::Punctuator_LeftParen },
        { "parameter-declaration-declaration-specifiers-suffix", TokType::Punctuator_Star },
        { "parameter-list_op_24", TokType::Punctuator_Comma },
        { "parameter-list_rr_op_25", TokType::Punctuator_Comma },
        { "specifier-qualifier-list", TokType::Keyword__Atomic },
        { "specifier-qualifier-list_op_5", TokType::Identifier },
        { "specifier-qualifier-list_op_6", TokType::Identifier },
        { "struct-declarator", TokType::Punctuator_LeftParen },
        { "struct-declarator", TokType::Punctuator_Star },
        { "struct-declarator", TokType::Identifier },
    };

    // TODO is it expected?
    // find the index of the option whose right-side contains the given symbol
    static auto FindOptionIndex(vector<SimpleRightSide> const& options, String const& symbol) -> int
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
        return -1;
    }

    // find the index of the empty option (right-side is empty)
    static auto FindEmptyOptionIndex(vector<SimpleRightSide> const& options) -> int
    {
        for (int i = 0; i < static_cast<int>(options.size()); ++i)
        {
            if (options[i].empty())
            {
                return i;
            }
        }
        return -1;
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

    // check if an identifier is a typedef-name in the given context's symbol table
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
        return conflicts.contains({ nontermin, static_cast<TokType>(tokType) });
    }

    // TODO remove the Result pass in LLParser side
    template <template <typename> class ActualStream, typename Token>
        requires Stream<ActualStream, Token>
    auto Resolve(stack<SyntaxTreeNode<Token, Context>*> workingNodes, String nontermin, TokType tokType, vector<SimpleRightSide> const& options, ActualStream<Token>& stream) const -> expected<int, ParseFailResult>
    {
        using std::format;
        using std::map;
        using ResolveFunc = auto (C11ConflictResolver::*)(stack<SyntaxTreeNode<Token, Context>*>&, vector<SimpleRightSide> const&, ActualStream<Token>&) const -> expected<int, ParseFailResult>;
        static map<pair<String, TokType>, ResolveFunc> const resolveMap
        {
            { { "declaration-specifiers_com_1",                         TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveTypeSpecifierAndTypeQualifier<ActualStream, Token> },
            { { "declaration-specifiers_op_2",                          TokType::Identifier },            &C11ConflictResolver::ResolveDeclarationSpecifiersAndInitDeclaratorList<ActualStream, Token> },
            { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffix<ActualStream, Token> },
            { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword_Const },         &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffix<ActualStream, Token> },
            { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword_Restrict },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffix<ActualStream, Token> },
            { { "direct-abstract-declarator-[-suffix",                  TokType::Keyword_Volatile },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorBracketSuffix<ActualStream, Token> },
            { { "direct-abstract-declarator_rr-[-suffix",               TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "direct-abstract-declarator_rr-[-suffix",               TokType::Keyword_Const },         &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "direct-abstract-declarator_rr-[-suffix",               TokType::Keyword_Restrict },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "direct-abstract-declarator_rr-[-suffix",               TokType::Keyword_Volatile },      &C11ConflictResolver::ResolveDirectAbstractDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "direct-declarator_rr-(-suffix",                        TokType::Identifier },            &C11ConflictResolver::ResolveDirectDeclaratorRrParenSuffix<ActualStream, Token> },
            { { "direct-declarator_rr-[-suffix",                        TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "direct-declarator_rr-[-suffix",                        TokType::Keyword_Const },         &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "direct-declarator_rr-[-suffix",                        TokType::Keyword_Restrict },      &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "direct-declarator_rr-[-suffix",                        TokType::Keyword_Volatile },      &C11ConflictResolver::ResolveDirectDeclaratorRrBracketSuffix<ActualStream, Token> },
            { { "enum-specifier-enum-suffix",                           TokType::Identifier },            &C11ConflictResolver::ResolveEnumSpecifierEnumSuffix<ActualStream, Token> },
            { { "enumerator-list_op_12",                                TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveEnumeratorListComma<ActualStream, Token> },
            { { "enumerator-list_rr_op_13",                             TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveEnumeratorListRrComma<ActualStream, Token> },
            //{ { "init-declarator-list",                                 TokType::Punctuator_LeftParen },  &C11ConflictResolver::ResolveInitDeclaratorList<ActualStream, Token> },
            //{ { "init-declarator-list",                                 TokType::Punctuator_Star },       &C11ConflictResolver::ResolveInitDeclaratorList<ActualStream, Token> },
            //{ { "init-declarator-list",                                 TokType::Identifier },            &C11ConflictResolver::ResolveInitDeclaratorList<ActualStream, Token> },
            { { "initializer-list_op_40",                               TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveInitializerListComma<ActualStream, Token> },
            { { "initializer-list_rr_op_42",                            TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveInitializerListRrComma<ActualStream, Token> },
            { { "parameter-declaration-declaration-specifiers-suffix",  TokType::Punctuator_LeftParen },  &C11ConflictResolver::ResolveParameterDeclarationSuffix<ActualStream, Token> },
            { { "parameter-declaration-declaration-specifiers-suffix",  TokType::Punctuator_Star },       &C11ConflictResolver::ResolveParameterDeclarationSuffix<ActualStream, Token> },
            { { "parameter-list_op_24",                                 TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveParameterListComma<ActualStream, Token> },
            { { "parameter-list_rr_op_25",                              TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveParameterListRrComma<ActualStream, Token> },
            { { "specifier-qualifier-list",                             TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveSpecifierQualifierListAtomic<ActualStream, Token> },
            { { "specifier-qualifier-list_op_5",                        TokType::Identifier },            &C11ConflictResolver::ResolveSpecifierQualifierListIdentifier<ActualStream, Token> },
            { { "specifier-qualifier-list_op_6",                        TokType::Identifier },            &C11ConflictResolver::ResolveSpecifierQualifierListIdentifier<ActualStream, Token> },
            { { "struct-declarator",                                    TokType::Punctuator_LeftParen },  &C11ConflictResolver::ResolveStructDeclarator<ActualStream, Token> },
            { { "struct-declarator",                                    TokType::Punctuator_Star },       &C11ConflictResolver::ResolveStructDeclarator<ActualStream, Token> },
            { { "struct-declarator",                                    TokType::Identifier },            &C11ConflictResolver::ResolveStructDeclarator<ActualStream, Token> },
        };
        auto it = resolveMap.find({ nontermin, tokType });
        if (it != resolveMap.end())
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
    // Otherwise, check if next token looks like a declarator start ([ or () → continue declaration-specifiers.
    // Else, it's the start of init-declarator (empty option).
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDeclarationSpecifiersAndInitDeclaratorList(stack<SyntaxTreeNode<Tok, Context>*>& workingNodes, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        // First check: if the current identifier is a typedef-name, continue declaration-specifiers
        auto context = FindContextFromWorkingNodes(workingNodes);
        if (IsTypedefName(context, stream.Current().Value))
        {
            return FindOptionIndex(options, "declaration-specifiers");
        }

        while (stream.MoveNext())
        {
            auto t = stream.Current();

            if (t.Type == TokType::Punctuator_LeftBracket or t.Type == TokType::Punctuator_LeftParen)
            {
                return FindOptionIndex(options, "declaration-specifiers");
            }
            else if (t.Type == TokType::Punctuator_Semicolon)
            {
                return FindEmptyOptionIndex(options);
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve declaration-specifier and init-declarator-list conflict" });
    }

    // conflict: direct-abstract-declarator-[-suffix + type-qualifier
    // options: {... op_30 op_31 ] ...} vs {type-qualifier-list static assignment-expression ] ...}
    // Look ahead: if after type-qualifier we see "static", pick option with "static", else the other
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectAbstractDeclaratorBracketSuffix(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
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
                return FindOptionIndex(options, "direct-abstract-declarator_op_30");
            }
            if (t.Type == TokType::Punctuator_Comma or t.Type == TokType::Punctuator_Semicolon or t.Type == TokType::Punctuator_RightParen)
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-abstract-declarator bracket suffix" });
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-abstract-declarator bracket suffix" });
    }

	// TODO how to handle the conflict in right-recursive rules after removing the left-recursive rules. below conflict is coming from right-recursive rules
    // conflict: direct-abstract-declarator_rr-[-suffix + type-qualifier
    // Same pattern as direct-abstract-declarator-[-suffix
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectAbstractDeclaratorRrBracketSuffix(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
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
                return FindOptionIndex(options, "direct-abstract-declarator_rr_op_34");
            }
            if (t.Type == TokType::Punctuator_Comma or t.Type == TokType::Punctuator_Semicolon or t.Type == TokType::Punctuator_RightParen)
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-abstract-declarator_rr bracket suffix" });
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-abstract-declarator_rr bracket suffix" });
    }

    // conflict: direct-declarator_rr-(-suffix + Identifier
    // options: {"parameter-type-list", ...} vs {"direct-declarator_rr_op_20", ...}
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
        return FindOptionIndex(options, "direct-declarator_rr_op_20");
    }

    // conflict: direct-declarator_rr-[-suffix + type-qualifier
    // options: {op_16 op_17 ] ...} vs {type-qualifier-list static ...} vs {op_19 * ] ...}
    // Look ahead: if "static" → option with "static", if "*" → option with "*", else option with "op_16"
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectDeclaratorRrBracketSuffix(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        while (stream.MoveNext())
        {
            auto t = stream.Current();
            if (t.Type == TokType::Keyword_Static)
            {
                return FindOptionIndex(options, "static");
            }
            if (t.Type == TokType::Punctuator_Star)
            {
                return FindOptionIndex(options, "direct-declarator_rr_op_19");
            }
            if (t.Type == TokType::Punctuator_RightBracket)
            {
                return FindOptionIndex(options, "direct-declarator_rr_op_16");
            }
            if (t.Type == TokType::Punctuator_Comma or t.Type == TokType::Punctuator_Semicolon or t.Type == TokType::Punctuator_RightParen)
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-declarator_rr bracket suffix" });
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-declarator_rr bracket suffix" });
    }

    // conflict: enum-specifier-enum-suffix + Identifier
    // options: {enum-specifier_op_10 { enumerator-list ...} vs {Identifier}
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
            return FindOptionIndex(options, "enum-specifier_op_10"); // enum definition with { }
        }
        return FindOptionIndex(options, "Identifier"); // enum reference, just Identifier
    }

    // conflict: enumerator-list_op_12 + ","
    // options: {enumerator-list_rr} vs {} (empty)
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
            return FindOptionIndex(options, "enumerator-list_rr");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: enumerator-list_rr_op_13 + ","
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
            return FindOptionIndex(options, "enumerator-list_rr");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: initializer-list_op_40 + ","
    // options: {initializer-list_rr} vs {} (empty)
    // Look ahead: if next can start an initializer, continue, else trailing comma
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveInitializerListComma(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return FindEmptyOptionIndex(options);
        }
        auto t = stream.Current();
        if (t.Type == TokType::Punctuator_LeftBrace or t.Type == TokType::Punctuator_LeftParen or t.Type == TokType::Punctuator_Star or t.Type == TokType::Identifier or
            t.Type == TokType::IntgerConstant or t.Type == TokType::FloatingConstant or
            t.Type == TokType::CharacterConstant or t.Type == TokType::StringLiteral or
            t.Type == TokType::Punctuator_Exclamation or t.Type == TokType::Punctuator_Tilde or t.Type == TokType::Punctuator_Ampersand or t.Type == TokType::Punctuator_Minus or t.Type == TokType::Punctuator_Plus)
        {
            return FindOptionIndex(options, "initializer-list_rr");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: initializer-list_rr_op_42 + ","
    // Same pattern as initializer-list_op_40
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveInitializerListRrComma(stack<SyntaxTreeNode<Tok, Context>*>&, vector<SimpleRightSide> const& options, ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return FindEmptyOptionIndex(options);
        }
        auto t = stream.Current();
        if (t.Type == TokType::Punctuator_LeftBrace or t.Type == TokType::Punctuator_LeftParen or t.Type == TokType::Punctuator_Star or t.Type == TokType::Identifier or
            t.Type == TokType::IntgerConstant or t.Type == TokType::FloatingConstant or
            t.Type == TokType::CharacterConstant or t.Type == TokType::StringLiteral or
            t.Type == TokType::Punctuator_Exclamation or t.Type == TokType::Punctuator_Tilde or t.Type == TokType::Punctuator_Ampersand or t.Type == TokType::Punctuator_Minus or t.Type == TokType::Punctuator_Plus)
        {
            return FindOptionIndex(options, "initializer-list_rr");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: parameter-list_op_24 + ","
    // options: {parameter-list_rr} vs {} (empty)
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
            return FindOptionIndex(options, "parameter-list_rr");
        }
        return FindEmptyOptionIndex(options);
    }

    // conflict: parameter-list_rr_op_25 + ","
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
            return FindOptionIndex(options, "parameter-list_rr");
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
        auto context = FindContextFromWorkingNodes(workingNodes);

        if (stream.Current().Type == TokType::Punctuator_Star)
        {
            // Look ahead: is there a non-typedef Identifier after the pointer?
            // If so, it's a named pointer declarator (e.g. int *p).
            // Otherwise, abstract (e.g. int * or int *(*)(int)).
            if (stream.MoveNext() and stream.Current().Type == TokType::Identifier and not IsTypedefName(context, stream.Current().Value))
            {
                return FindOptionIndex(options, "declarator");
            }
            return FindOptionIndex(options, "abstract-declarator");
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
                return FindOptionIndex(options, "abstract-declarator");
            }
            // Identifier (non-typedef), *, (, [ — it's a parenthesized declarator (e.g. int (x), int (*p)(int))
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
                return FindOptionIndex(options, "struct-declarator_op_9");
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
            return FindOptionIndex(options, "struct-declarator_op_9");
        }
        return FindOptionIndex(options, "declarator");
    }
};

export
{
    class C11ConflictResolver;
}
