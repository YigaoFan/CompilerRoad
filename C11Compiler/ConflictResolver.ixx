export module ConflictResolver;

import std;
import Base;
import Parser;
import C11Spec;

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
public:
    auto Resolvable(String nontermin, int tokType) const -> bool
    {
        return conflicts.contains({ nontermin, static_cast<TokType>(tokType) });
    }

    template <typename Result, template <typename> class ActualStream, typename Token>
        requires Stream<ActualStream, Token>
    auto Resolve(stack<SyntaxTreeNode<Token, Result>*> workingNodes, String nontermin, TokType tokType, ActualStream<Token>& stream) const -> expected<int, ParseFailResult>
    {
        using std::format;
        using std::map;
        using ResolveFunc = auto (C11ConflictResolver::*)(ActualStream<Token>&) const -> expected<int, ParseFailResult>;
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
            { { "init-declarator-list",                                 TokType::Punctuator_LeftParen },  &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "init-declarator-list",                                 TokType::Punctuator_Star },       &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "init-declarator-list",                                 TokType::Identifier },            &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "initializer-list_op_40",                               TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveInitializerListComma<ActualStream, Token> },
            { { "initializer-list_rr_op_42",                            TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveInitializerListRrComma<ActualStream, Token> },
            { { "parameter-declaration-declaration-specifiers-suffix",  TokType::Punctuator_LeftParen },  &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "parameter-declaration-declaration-specifiers-suffix",  TokType::Punctuator_Star },       &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "parameter-list_op_24",                                 TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveParameterListComma<ActualStream, Token> },
            { { "parameter-list_rr_op_25",                              TokType::Punctuator_Comma },      &C11ConflictResolver::ResolveParameterListRrComma<ActualStream, Token> },
            { { "specifier-qualifier-list",                             TokType::Keyword__Atomic },       &C11ConflictResolver::ResolveSpecifierQualifierListAtomic<ActualStream, Token> },
            { { "specifier-qualifier-list_op_5",                        TokType::Identifier },            &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "specifier-qualifier-list_op_6",                        TokType::Identifier },            &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "struct-declarator",                                    TokType::Punctuator_LeftParen },  &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "struct-declarator",                                    TokType::Punctuator_Star },       &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
            { { "struct-declarator",                                    TokType::Identifier },            &C11ConflictResolver::TODO_Resolve<ActualStream, Token> },
        };
        auto it = resolveMap.find({ nontermin, tokType });
        if (it != resolveMap.end())
        {
            return (this->*it->second)(stream);
        }
        return unexpected(ParseFailResult{ .Message = format("unhandled conflict: {} with {}", nontermin, tokType) });
    }

    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto TODO_Resolve(ActualStream<Tok>&) const -> expected<int, ParseFailResult>
    {
        return 0;
    }

    // Change all usages of "Stream<Tok> auto&" to "auto&" and add a requires clause for Stream
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveTypeSpecifierAndTypeQualifier(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        // conflict on: "_Atomic"

        if (not stream.MoveNext())
        {
            return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve type-specifier and type-qualifier conflict" });
        }

        if (stream.Current().Value == "(")
        {
            return 0;
        }
        return 1;
    }

    // the current token is conflict token, the caller ensure the position recover
    // stream.RollbackTo(pos);
    // other place config the conflict rules
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDeclarationSpecifiersAndInitDeclaratorList(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        // conflict on: Identifier
        // declaration-specifiers_op_2 -> declaration-specifiers | empty
        // If Identifier is a typedef-name, continue with declaration-specifiers
        // Otherwise, it's the start of init-declarator
        // TODO: need symbol table to determine if Identifier is typedef-name

        while (stream.MoveNext())
        {
            auto t = stream.Current();

            if (t.Value == "[" or t.Value == "(")
            {
                return 1;
            }
            else if (t.Value == ";")
            {
                return 0;
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve declaration-specifier and init-declarator-list conflict" });
    }

    // direct-abstract-declarator-[-suffix + type-qualifier
    // rule 0: [ type-qualifier-list [assignment-expression] ] direct-abstract-declarator_rr
    // rule 1: [ type-qualifier-list static assignment-expression ] direct-abstract-declarator_rr
    // Look ahead: if after type-qualifier we see "static", rule 1, else rule 0
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectAbstractDeclaratorBracketSuffix(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        // skip the type-qualifier token we already saw
        // look for "static" keyword
        while (stream.MoveNext())
        {
            auto t = stream.Current();
            if (t.Value == "static")
            {
                return 1; // rule 1: type-qualifier-list static assignment-expression
            }
            if (t.Value == "]")
            {
                return 0; // rule 0: type-qualifier-list [assignment-expression]
            }
            if (t.Value == "," or t.Value == ";" or t.Value == ")")
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-abstract-declarator bracket suffix" });
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-abstract-declarator bracket suffix" });
    }

    // direct-abstract-declarator_rr-[-suffix + type-qualifier
    // Same pattern as direct-abstract-declarator-[-suffix
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectAbstractDeclaratorRrBracketSuffix(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        while (stream.MoveNext())
        {
            auto t = stream.Current();
            if (t.Value == "static")
            {
                return 1;
            }
            if (t.Value == "]")
            {
                return 0;
            }
            if (t.Value == "," or t.Value == ";" or t.Value == ")")
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-abstract-declarator_rr bracket suffix" });
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-abstract-declarator_rr bracket suffix" });
    }

    // direct-declarator_rr-(-suffix + Identifier
    // rule 0: ( parameter-type-list ) direct-declarator_rr
    // rule 1: ( identifier-list ) direct-declarator_rr
    // Need symbol table to determine if Identifier is typedef-name
    // For now, assume rule 0 (parameter-type-list)
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectDeclaratorRrParenSuffix(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        // TODO: need symbol table
        // If Identifier is a typedef-name, it's parameter-type-list
        // Otherwise, it's identifier-list
        return 0; // default to parameter-type-list
    }

    // direct-declarator_rr-[-suffix + type-qualifier
    // rule 0: [ type-qualifier-list [assignment-expression] ] direct-declarator_rr
    // rule 1: [ type-qualifier-list static assignment-expression ] direct-declarator_rr
    // rule 2: [ type-qualifier-list * ] direct-declarator_rr
    // Look ahead: if "static" → rule 1, if "*" → rule 2, else rule 0
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveDirectDeclaratorRrBracketSuffix(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        while (stream.MoveNext())
        {
            auto t = stream.Current();
            if (t.Value == "static")
            {
                return 1; // rule 1: type-qualifier-list static assignment-expression
            }
            if (t.Value == "*")
            {
                return 2; // rule 2: type-qualifier-list *
            }
            if (t.Value == "]")
            {
                return 0; // rule 0: type-qualifier-list [assignment-expression]
            }
            if (t.Value == "," or t.Value == ";" or t.Value == ")")
            {
                return unexpected(ParseFailResult{ .Message = "unexpected token when resolve direct-declarator_rr bracket suffix" });
            }
        }
        return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve direct-declarator_rr bracket suffix" });
    }

    // enum-specifier-enum-suffix + Identifier
    // rule 0: Identifier { enumerator-list } (enum definition)
    // rule 1: Identifier (enum reference)
    // Look ahead: if next is "{", rule 0, else rule 1
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveEnumSpecifierEnumSuffix(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve enum-specifier-enum-suffix" });
        }
        if (stream.Current().Value == "{")
        {
            return 0; // enum definition
        }
        return 1; // enum reference
    }

    // enumerator-list_op_12 + ","
    // rule 0: enumerator-list_rr (continue)
    // rule 1: empty (end of list, trailing comma)
    // Look ahead: if next can start an enumerator (Identifier), rule 0, else rule 1
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveEnumeratorListComma(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return 1; // trailing comma at end
        }
        if (stream.Current().Type == TokType::Identifier)
        {
            return 0; // continue with next enumerator
        }
        return 1; // trailing comma
    }

    // enumerator-list_rr_op_13 + ","
    // Same pattern as enumerator-list_op_12
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveEnumeratorListRrComma(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return 1;
        }
        if (stream.Current().Type == TokType::Identifier)
        {
            return 0;
        }
        return 1;
    }

    // initializer-list_op_40 + ","
    // rule 0: initializer-list_rr (continue)
    // rule 1: empty (end of list, trailing comma)
    // Look ahead: if next can start an initializer, rule 0, else rule 1
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveInitializerListComma(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return 1;
        }
        auto t = stream.Current();
        // Check if next token can start an initializer
        // initializer -> assignment-expression | "{" initializer-list "}"
        if (t.Value == "{" or t.Value == "(" or t.Value == "*" or t.Type == TokType::Identifier or
            t.Type == TokType::IntgerConstant or t.Type == TokType::FloatingConstant or
            t.Type == TokType::CharacterConstant or t.Type == TokType::StringLiteral or
            t.Value == "!" or t.Value == "~" or t.Value == "&" or t.Value == "-" or t.Value == "+")
        {
            return 0; // continue
        }
        return 1; // trailing comma
    }

    // initializer-list_rr_op_42 + ","
    // Same pattern as initializer-list_op_40
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveInitializerListRrComma(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return 1;
        }
        auto t = stream.Current();
        if (t.Value == "{" or t.Value == "(" or t.Value == "*" or t.Type == TokType::Identifier or
            t.Type == TokType::IntgerConstant or t.Type == TokType::FloatingConstant or
            t.Type == TokType::CharacterConstant or t.Type == TokType::StringLiteral or
            t.Value == "!" or t.Value == "~" or t.Value == "&" or t.Value == "-" or t.Value == "+")
        {
            return 0;
        }
        return 1;
    }

    // parameter-list_op_24 + ","
    // rule 0: parameter-list_rr (continue)
    // rule 1: empty (end of list)
    // Look ahead: if next can start a parameter-declaration, rule 0, else rule 1
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveParameterListComma(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return 1;
        }
        auto t = stream.Current();
        // Check if next token can start a declaration-specifier
        // storage-class-specifier, type-specifier, type-qualifier, function-specifier, alignment-specifier
        if (t.Type == TokType::Identifier or
            t.Value == "typedef" or t.Value == "extern" or t.Value == "static" or
            t.Value == "_Thread_local" or t.Value == "auto" or t.Value == "register" or
            t.Value == "void" or t.Value == "char" or t.Value == "short" or t.Value == "int" or
            t.Value == "long" or t.Value == "float" or t.Value == "double" or
            t.Value == "signed" or t.Value == "unsigned" or t.Value == "_Bool" or t.Value == "_Complex" or
            t.Value == "_Atomic" or t.Value == "struct" or t.Value == "union" or t.Value == "enum" or
            t.Value == "const" or t.Value == "restrict" or t.Value == "volatile" or
            t.Value == "inline" or t.Value == "_Noreturn" or t.Value == "_Alignas")
        {
            return 0; // continue
        }
        return 1; // end of list
    }

    // parameter-list_rr_op_25 + ","
    // Same pattern as parameter-list_op_24
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveParameterListRrComma(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return 1;
        }
        auto t = stream.Current();
        if (t.Type == TokType::Identifier or
            t.Value == "typedef" or t.Value == "extern" or t.Value == "static" or
            t.Value == "_Thread_local" or t.Value == "auto" or t.Value == "register" or
            t.Value == "void" or t.Value == "char" or t.Value == "short" or t.Value == "int" or
            t.Value == "long" or t.Value == "float" or t.Value == "double" or
            t.Value == "signed" or t.Value == "unsigned" or t.Value == "_Bool" or t.Value == "_Complex" or
            t.Value == "_Atomic" or t.Value == "struct" or t.Value == "union" or t.Value == "enum" or
            t.Value == "const" or t.Value == "restrict" or t.Value == "volatile" or
            t.Value == "inline" or t.Value == "_Noreturn" or t.Value == "_Alignas")
        {
            return 0;
        }
        return 1;
    }

    // specifier-qualifier-list + _Atomic
    // rule 0: type-specifier specifier-qualifier-list_op_5
    // rule 1: type-qualifier specifier-qualifier-list_op_6
    // If _Atomic is followed by "(", it's atomic-type-specifier (type-specifier), else type-qualifier
    template <template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto ResolveSpecifierQualifierListAtomic(ActualStream<Tok>& stream) const -> expected<int, ParseFailResult>
    {
        if (!stream.MoveNext())
        {
            return unexpected(ParseFailResult{ .Message = "input stream is empty when resolve specifier-qualifier-list _Atomic conflict" });
        }
        if (stream.Current().Value == "(")
        {
            return 0; // atomic-type-specifier (type-specifier)
        }
        return 1; // type-qualifier
    }
};

export
{
    class C11ConflictResolver;
}