#pragma once
#include "scripting/Syntax.h"
namespace game::script {
// Only async bodies are lowered. Synchronous scripts keep the existing evaluator.
enum class AsyncOp { Eval,Call,Member,Unary,Binary,List,Await,Assign,Constant,Discard,Return,
    Branch,Jump,Enter,Leave,ForBegin,ForNext,And,Or,Statement };
struct AsyncInstruction { AsyncOp op;NodePtr node;size_t argument=0; };
struct AsyncProgram { std::vector<AsyncInstruction> code; };
std::shared_ptr<const AsyncProgram> compileAsync(const Block& body);
}
