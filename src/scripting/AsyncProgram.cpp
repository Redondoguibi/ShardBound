#include "scripting/AsyncProgram.h"
namespace game::script {
namespace {
bool awaits(const NodePtr& node) {
    if(node->kind==Kind::Await) return true;
    for(const auto& child:node->children) if(awaits(child)) return true;
    return false;
}
struct Compiler {
    AsyncProgram result;
    size_t emit(AsyncOp op,const NodePtr& node,size_t argument=0) {
        if(result.code.size()>=65536) fail(node->at,"SCRIPT341","Async function is too large");
        result.code.push_back({op,node,argument});return result.code.size()-1;
    }
    void expression(const NodePtr& n) {
        if(!awaits(n)) {emit(AsyncOp::Eval,n);return;}
        if(n->kind==Kind::Binary && (n->text=="and" || n->text=="or")) {
            expression(n->children[0]);const auto branch=emit(n->text=="and"?AsyncOp::And:AsyncOp::Or,n);
            expression(n->children[1]);emit(AsyncOp::Unary,n); // boolean validation of the right operand
            result.code[branch].argument=result.code.size();return;
        }
        for(const auto& child:n->children) expression(child);
        switch(n->kind) {
        case Kind::Await:emit(AsyncOp::Await,n);break;
        case Kind::Call:emit(AsyncOp::Call,n,n->children.size()-1);break;
        case Kind::Member:emit(AsyncOp::Member,n);break;
        case Kind::Unary:emit(AsyncOp::Unary,n);break;
        case Kind::Binary:emit(AsyncOp::Binary,n);break;
        case Kind::List:emit(AsyncOp::List,n,n->children.size());break;
        default:fail(n->at,"SCRIPT341","Unsupported async expression");
        }
    }
    void block(const Block& nodes) {
        for(const auto& n:nodes) switch(n->kind) {
        case Kind::Assignment:
            expression(n->children[1]);
            if(n->children[0]->kind==Kind::Member) expression(n->children[0]->children[0]);
            emit(AsyncOp::Assign,n);break;
        case Kind::Constant:expression(n->children[0]);emit(AsyncOp::Constant,n);break;
        case Kind::Expression:expression(n->children[0]);emit(AsyncOp::Discard,n);break;
        case Kind::Return:
            for(const auto& value:n->children) expression(value);
            emit(AsyncOp::Return,n,n->children.size());break;
        case Kind::If: {
            expression(n->children[0]);const auto branch=emit(AsyncOp::Branch,n);
            emit(AsyncOp::Enter,n);block(n->body);emit(AsyncOp::Leave,n);
            const auto end=emit(AsyncOp::Jump,n);result.code[branch].argument=result.code.size();
            emit(AsyncOp::Enter,n);block(n->alternative);emit(AsyncOp::Leave,n);
            result.code[end].argument=result.code.size();break;
        }
        case Kind::For: {
            expression(n->children[0]);const auto begin=emit(AsyncOp::ForBegin,n);
            block(n->body);emit(AsyncOp::ForNext,n,begin+1);result.code[begin].argument=result.code.size();break;
        }
        default:emit(AsyncOp::Statement,n);break;
        }
    }
};
}
std::shared_ptr<const AsyncProgram> compileAsync(const Block& body) {
    Compiler compiler;compiler.block(body);return std::make_shared<const AsyncProgram>(std::move(compiler.result));
}
}
