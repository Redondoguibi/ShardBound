#pragma once
#include "core/Diagnostic.h"
#include <memory>
#include <string>
#include <vector>
namespace game::script {
struct Location { std::string file; unsigned line=1,column=1; };
[[noreturn]] inline void fail(const Location& at,std::string code,std::string message) {
    throw DiagnosticError({std::move(code),std::move(message),at.file,{},{},at.line,at.column});
}
enum class TokenKind { Name,Number,String,Punctuation,Newline,Indent,Dedent,End };
struct Token { TokenKind kind; std::string text; Location at; };
std::vector<Token> lex(std::string_view source,const std::string& file);
enum class Kind { Import,Category,Declaration,Section,Property,Assignment,Constant,Function,Return,If,For,
    List,Call,Member,Binary,Unary,Number,String,Boolean,Null,Identifier,Modify,Expression };
struct Node;
using NodePtr=std::shared_ptr<Node>;
using Block=std::vector<NodePtr>;
struct Parameter { std::string name; NodePtr defaultValue; };
struct Node {
    Kind kind;
    Location at;
    std::string text,extra;
    std::vector<NodePtr> children;
    Block body,alternative;
    std::vector<Parameter> parameters;
};
Block parse(std::string_view source,const std::string& file);
}
