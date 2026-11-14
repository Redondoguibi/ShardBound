#include "scripting/Syntax.h"
#include <map>
#include <cmath>
namespace game::script {
namespace {
class Parser {
    std::vector<Token> tokens_; size_t cursor_=0; unsigned depth_=0; bool inAsync_=false;
    const Token& peek(size_t offset=0) const { return tokens_.at(std::min(cursor_+offset,tokens_.size()-1)); }
    bool is(std::string_view text) const { return peek().kind!=TokenKind::String && peek().kind!=TokenKind::Number && peek().text==text; }
    bool accept(std::string_view text) { if(!is(text)) return false; ++cursor_; return true; }
    Token take() { return tokens_.at(cursor_++); }
    Token expect(std::string_view text) { if(!is(text)) fail(peek().at,"SCRIPT201","Expected '"+std::string(text)+"', got '"+peek().text+"'"); return take(); }
    Token name() { if(peek().kind!=TokenKind::Name) fail(peek().at,"SCRIPT201","Expected identifier"); return take(); }
    NodePtr node(Kind kind,Token token) { auto result=std::make_shared<Node>(); result->kind=kind; result->at=token.at; result->text=token.text; return result; }
    void newline() { if(!accept("<newline>") && !is("<end>") && !is("}")) fail(peek().at,"SCRIPT201","Expected end of line"); }
    Block suite(bool declarative=false) {
        const bool outerAsync=inAsync_;if(declarative) inAsync_=false;
        if(++depth_>128) fail(peek().at,"SCRIPT201","Block nesting limit exceeded");
        expect(":"); expect("<newline>"); expect("<indent>");
        Block result;
        while(!is("<dedent>") && !is("<end>")) { if(accept("<newline>")) continue; result.push_back(statement(declarative)); }
        expect("<dedent>"); --depth_;inAsync_=outerAsync;return result;
    }
    Block structured() {
        const bool outerAsync=inAsync_;inAsync_=false;
        if(++depth_>128) fail(peek().at,"SCRIPT201","Block nesting limit exceeded");
        expect("{"); expect("<newline>");
        const bool indented=accept("<indent>"); Block result;
        while(!is("}") && !is("<end>") && !(indented && is("<dedent>"))) {
            if(accept("<newline>")) continue; result.push_back(statement(true));
        }
        if(indented) expect("<dedent>"); expect("}"); newline(); --depth_;inAsync_=outerAsync;return result;
    }
    NodePtr function(bool event=false,bool asynchronous=false) {
        const auto token=event ? peek() : expect("func");
        auto result=node(Kind::Function,token); result->text=name().text; result->extra=event ? "event" : "";
        expect("("); bool defaults=false;const bool outerAsync=inAsync_;inAsync_=false;
        if(!is(")")) do {
            Parameter param{name().text,{}};
            for(const auto& prior:result->parameters) if(prior.name==param.name) fail(token.at,"SCRIPT202","Duplicate parameter");
            if(accept("=")) { defaults=true; param.defaultValue=expression(); }
            else if(defaults) fail(token.at,"SCRIPT202","Required parameter follows default parameter");
            result->parameters.push_back(std::move(param));
        } while(accept(","));
        expect(")");inAsync_=asynchronous;result->asynchronous=asynchronous;result->body=suite();inAsync_=outerAsync;return result;
    }
    bool eventFunction() const {
        if(peek().kind!=TokenKind::Name || peek(1).text!="(") return false;
        int depth=0;
        for(size_t i=1;cursor_+i<tokens_.size();++i) {
            if(peek(i).text=="(") ++depth;
            if(peek(i).text==")" && --depth==0) return peek(i+1).text==":";
            if(peek(i).kind==TokenKind::End) break;
        }
        return false;
    }
    NodePtr statement(bool declarative=false) {
        auto token=peek();
        if(accept("@")) {
            auto result=node(Kind::Category,token); result->text=name().text;
            while(accept("/")) result->text+="/"+name().text;
            newline(); return result;
        }
        if(accept("import")) {
            auto result=node(Kind::Import,token);
            if(peek().kind!=TokenKind::String) fail(peek().at,"SCRIPT201","Import requires a string path");
            result->text=take().text; newline(); return result;
        }
        if(is("async") && peek(1).text=="func") { take();return function(false,true); }
        if(is("func")) return function();
        if(eventFunction()) return function(true);
        if(accept("if")) {
            auto result=node(Kind::If,token); result->children.push_back(expression()); result->body=suite();
            if(accept("else")) result->alternative=suite(); return result;
        }
        if(accept("for")) {
            auto result=node(Kind::For,token); result->text=name().text; expect("in"); result->children.push_back(expression()); result->body=suite(); return result;
        }
        if(accept("return")) {
            auto result=node(Kind::Return,token);
            if(!is("<newline>") && !is("<end>")) do { result->children.push_back(expression()); } while(accept(","));
            newline(); return result;
        }
        if(accept("modify")) {
            auto result=node(Kind::Modify,token); result->text=take().text; result->body=structured(); return result;
        }
        if(declarative && peek().kind==TokenKind::Name && peek(1).text==":") {
            auto result=node(Kind::Section,take()); result->body=suite(true); return result;
        }
        if(is("await") && !inAsync_ && peek(1).kind==TokenKind::Name && peek(2).text=="(") fail(peek().at,"SCRIPT340","await requires an async function body");
        if(!(is("await") && inAsync_) && peek().kind==TokenKind::Name && peek(1).kind==TokenKind::Name && peek(1).text!="and" && peek(1).text!="or") {
            auto type=take(); auto result=node(Kind::Declaration,name()); result->extra=type.text;
            if(accept("from")) {const bool outerAsync=inAsync_;inAsync_=false;result->children.push_back(expression());inAsync_=outerAsync;}
            result->body=is("{") ? structured() : suite(true); return result;
        }
        if(accept(".")) {
            auto result=node(declarative ? Kind::Property : Kind::Constant,name());
            if(accept("=")) result->children.push_back(expression());
            else if(!declarative) fail(token.at,"SCRIPT201","Constant requires an initializer");
            newline(); return result;
        }
        auto lhs=expression();
        if(accept("=")) {
            if(lhs->kind!=Kind::Identifier && lhs->kind!=Kind::Member) fail(token.at,"SCRIPT201","Invalid assignment target");
            auto result=node(Kind::Assignment,token); result->children={lhs,expression()}; newline(); return result;
        }
        auto result=node(Kind::Expression,token); result->children={lhs}; newline(); return result;
    }
    static int precedence(std::string_view op) {
        if(op=="or") return 1; if(op=="and") return 2;
        if(op=="==" || op=="!=") return 3;
        if(op=="<" || op=="<=" || op==">" || op==">=") return 4;
        if(op=="+" || op=="-") return 5;
        if(op=="*" || op=="/" || op=="%") return 6; return 0;
    }
    NodePtr expression(int minimum=1) {
        if(++depth_>128) fail(peek().at,"SCRIPT201","Expression nesting limit exceeded");
        NodePtr lhs;
        if(is("await") && inAsync_) { auto op=take();lhs=node(Kind::Await,op);lhs->children={expression(7)}; }
        else if(is("await") && (peek(1).kind==TokenKind::Name || peek(1).kind==TokenKind::Number || peek(1).kind==TokenKind::String) && precedence(peek(1).text)==0) fail(peek().at,"SCRIPT340","await requires an async function body");
        else if(is("not") || is("-") || is("+")) { auto op=take(); lhs=node(Kind::Unary,op); lhs->children={expression(op.text=="not" ? 3 : 7)}; }
        else lhs=primary();
        while(precedence(peek().text)>=minimum) {
            auto op=take(); auto result=node(Kind::Binary,op); result->children={lhs,expression(precedence(op.text)+1)}; lhs=result;
        }
        --depth_; return lhs;
    }
    NodePtr primary() {
        const auto token=take(); NodePtr result;
        if(token.kind==TokenKind::Number) {
            result=node(Kind::Number,token);
            try {result->numericValue=std::stod(token.text);}catch(const std::exception&) {fail(token.at,"SCRIPT301","Invalid numeric literal");}
            if(!std::isfinite(result->numericValue)) fail(token.at,"SCRIPT301","Number is not finite");
        }
        else if(token.kind==TokenKind::String) result=node(Kind::String,token);
        else if(token.text=="true" || token.text=="false") result=node(Kind::Boolean,token);
        else if(token.text=="null") result=node(Kind::Null,token);
        else if(token.text=="(") { result=expression(); expect(")"); }
        else if(token.text=="[") {
            result=node(Kind::List,token); if(!is("]")) do { result->children.push_back(expression()); } while(accept(",")); expect("]");
        } else if(token.kind==TokenKind::Name) result=node(Kind::Identifier,token);
        else fail(token.at,"SCRIPT201","Expected expression, got "+token.text);
        while(true) {
            if(accept(".")) { auto member=node(Kind::Member,name()); member->children={result}; result=member; }
            else if(accept("(")) {
                auto call=node(Kind::Call,token); call->children={result};
                if(!is(")")) do { call->children.push_back(expression()); } while(accept(","));
                expect(")"); result=call;
            } else break;
        }
        return result;
    }
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
    Block module() { Block result; while(!is("<end>")) { if(accept("<newline>")) continue; result.push_back(statement()); } return result; }
};
}
Block parse(std::string_view source,const std::string& file) { return Parser(lex(source,file)).module(); }
}
