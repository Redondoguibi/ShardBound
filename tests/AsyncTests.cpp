#include "scripting/Runtime.h"
#include "scripting/DataConversion.h"
#include "scripting/ScriptCheck.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace game::script;
namespace {
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
template<class F> void rejects(F action,const std::string& code) {
    try {action();}catch(const game::DiagnosticError& e) {if(e.diagnostic.code==code) return;throw;}
    throw std::runtime_error("Expected diagnostic "+code);
}
void drain(Runtime& runtime,int maximum=1000) {
    for(int i=0;i<maximum;++i) {
        auto errors=runtime.advanceTasks(.01);if(!errors.empty()) throw game::DiagnosticError(errors.front());
        auto snapshot=fromData(runtime.debugSnapshot());
        if(std::get<std::shared_ptr<List>>(snapshot.member("tasks",{}).data)->values.empty()) return;
    }
    throw std::runtime_error("Tasks did not drain");
}
void semantics() {
    Runtime r;
    r.bind("record",native([](const auto&,const auto&) {return object();}));
    r.execute("test",R"SL(
@Globals
progress = 0
async func child(value = 3):
    await next_frame()
    return value * 2
async func parent():
    progress = 1
    left = child()
    right = child(4)
    values = [await left, await right]
    total = 0
    for value in values:
        if value > 0:
            await next_frame()
            total = total + value
    progress = 2
    return total, await child(5)
job = parent()
)SL","async-contract.sl");
    check(r.global("test","progress").number()==0,"Async call ran inline");
    check(r.advanceTasks(0).empty(),"Initial resume failed");
    check(r.global("test","progress").number()==1,"Task did not start");
    check(!r.global("test","job").member("done",{}).boolean(),"Await failed to suspend");
    drain(r);
    const auto result=r.global("test","job").member("result",{});
    const auto values=std::get<std::shared_ptr<List>>(result.data)->values;
    check(values.size()==2 && values[0].number()==14 && values[1].number()==10,"Nested tasks/loop scopes/defaults/multiple returns failed");
    check(r.global("test","progress").number()==2,"Continuation lost globals");
    rejects([&]{toData(r.global("test","job"));},"EXT110");
    r.execute("test",R"SL(
@Globals
effects = 0
async func effect():
    effects = effects + 1
    return true
async func expressions():
    a = false and await effect()
    b = true or await effect()
    c = true and await effect()
    d = false or await effect()
    return a == false and b and c and d
expression_job = expressions()
)SL");
    drain(r);check(r.global("test","effects").number()==2,"Await broke short-circuit evaluation");
    check(r.global("test","expression_job").member("result",{}).boolean(),"Async boolean expression failed");
    r.execute("test",R"SL(
@Globals
target = record()
target.value = 0
async func object_task():
    await next_frame()
    return target
async func compound():
    (await object_task()).value = await child(7)
    for value in await list_task():
        target.value = target.value + value
    if await bool_task():
        target.value = -await child(2) + target.value
    else:
        target.value = 999
    return target
async func list_task():
    return [1, 2]
async func bool_task():
    return true
compound_job = compound()
)SL");
    drain(r);
    // object_task returns a snapshot; assigning to it must not mutate target.
    check(r.global("test","target").member("value",{}).number()==-1,"Awaited assignment/loops/condition/unary failed");
    auto copy=r.global("test","compound_job").member("result",{});copy.setMember("value",Value(999.0),{});
    check(r.global("test","compound_job").member("result",{}).member("value",{}).number()==-1,"Task result alias escaped snapshot");
}
void timersCancellationErrors() {
    Runtime r;
    r.execute("test",R"SL(
@Globals
ticks = 0
async func timed():
    await sleep(0.5)
    ticks = ticks + 1
job = timed()
)SL");
    check(r.advanceTasks(0).empty(),"Timer start failed");
    check(r.advanceTasks(.49).empty() && r.global("test","ticks").number()==0,"Timer resumed early");
    check(r.advanceTasks(.01).empty() && r.global("test","ticks").number()==1,"Timer did not use game time");
    r.execute("test","@Globals\ncancelled = timed()\ntask_cancel(cancelled)\n");
    drain(r);check(r.global("test","cancelled").member("cancelled",{}).boolean(),"Queued cancellation failed");
    check(r.global("test","ticks").number()==1,"Cancelled task ran");
    r.execute("test",R"SL(
@Globals
async func bad():
    await next_frame()
    return 1 / 0
async func wait_bad():
    return await bad()
failure = wait_bad()
)SL","failure.sl");
    size_t errors=0;
    for(int i=0;i<8;++i) for(const auto& e:r.advanceTasks(.01)) {check(e.code=="SCRIPT305" && e.file=="failure.sl" && e.line==5 && e.mod=="test","Task error lost source or owner");++errors;}
    check(errors==2 && r.global("test","failure").member("failed",{}).boolean(),"Task failure did not propagate once per task");
    rejects([&]{r.global("test","failure").member("result",{});},"SCRIPT305");
    r.execute("test","@Globals\nremaining = timed()\n");r.advanceTasks(0);r.cancelTasks();
    check(r.global("test","remaining").member("cancelled",{}).boolean(),"Session cancellation failed");
    r.execute("test",R"SL(
@Globals
victim = timed()
async func observer():
    await victim
waiting_job = observer()
)SL");
    r.advanceTasks(0);r.execute("test","task_cancel(victim)\n");
    auto cancellation=r.advanceTasks(0);check(cancellation.size()==1 && cancellation[0].code=="SCRIPT344","Awaited cancellation did not propagate");r.cancelTasks();
}
void validation() {
    rejects([]{parse("await sleep(1)\n","bad.sl");},"SCRIPT340");
    rejects([]{parse("async func f():\n    func g():\n        return await sleep(1)\n","bad.sl");},"SCRIPT340");
    rejects([]{parse("async func f(x = await sleep(1)):\n    return x\n","bad.sl");},"SCRIPT340");
    Runtime r;
    r.execute("test","async func invalid():\n    await 3\n@Globals\ninvalid_job = invalid()\n");
    auto errors=r.advanceTasks(0);check(errors.size()==1 && errors[0].code=="SCRIPT342","Await accepted non-task");
    r.execute("test","async func f():\n    return 1\n");
    rejects([&]{r.subscribe("test",r.global("test","f"),true,"test");},"SCRIPT340");
    rejects([&]{r.execute("test","register_system(\"test:bad\", \"update\", f)\n");},"SCRIPT340");
    rejects([&]{r.execute("test","sleep(-1)\n");},"SCRIPT342");
    rejects([&]{r.advanceTasks(std::numeric_limits<double>::quiet_NaN());},"SCRIPT346");
    r.execute("legacy","@Globals\nasync = 2\nawait = 3\nx = async + await\ny = await + 3\nif await == 3:\n    x = x + 0\n");
    check(r.global("legacy","x").number()==5,"Contextual keywords broke existing identifiers");
    r.execute("test","@Globals\nself = null\nasync func cycle():\n    await self\nself = cycle()\n");
    errors=r.advanceTasks(0);check(errors.size()==1 && errors[0].code=="SCRIPT343","Self-await was not rejected");
    r.execute("test","@Globals\na = null\nb = null\nasync func first():\n    await b\nasync func second():\n    await a\na = first()\nb = second()\n");
    errors=r.advanceTasks(0);check(errors.size()==1 && errors[0].code=="SCRIPT343","Indirect await cycle was not rejected");
    errors=r.advanceTasks(0);check(errors.size()==1 && errors[0].code=="SCRIPT343","Cycle failure did not propagate");
    r.execute("test","async func budget():\n    return expensive()\nfunc expensive():\n    value = 0\n    for a in number_range(4096):\n        for b in number_range(4096):\n            value = value + b\n@Globals\nexhausted = budget()\n");
    errors=r.advanceTasks(0);check(errors.size()==1 && errors[0].code=="SCRIPT304","Async synchronous work bypassed instruction budget");
    Runtime other;other.bind("foreign",r.global("test","exhausted"));
    other.execute("other","async func f():\n    await foreign\n@Globals\njob = f()\n");
    errors=other.advanceTasks(0);check(errors.size()==1 && errors[0].code=="SCRIPT342","Cross-session task accepted");
    const auto example=checkScripts("examples/async-tasks","main.sl");check(example.diagnostics.empty() && example.files==1,"Public async example failed static checking");
}
void fairnessAndLimits() {
    Runtime r;
    r.execute("test",R"SL(
@Globals
finished = 0
async func work():
    total = 0
    for a in number_range(1024):
        for b in number_range(8):
            total = total + b
    finished = finished + 1
    return total
jobs = []
for index in number_range(32):
    list_append(jobs, work())
)SL");
    r.advanceTasks(0);check(r.global("test","finished").number()==0,"Large task monopolized scheduler");
    drain(r,4000);check(r.global("test","finished").number()==32,"Task scheduler starvation");
    r.execute("limits","async func wait():\n    await sleep(100)\nfor i in number_range(64):\n    wait()\n");
    rejects([&]{r.execute("limits","wait()\n");},"SCRIPT345");r.cancelTasks();
}
}
int main() {
    try {semantics();timersCancellationErrors();validation();fairnessAndLimits();std::cout<<"PASS async syntax, continuation, clocks, cancellation, diagnostics, limits and fairness\n";return 0;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
