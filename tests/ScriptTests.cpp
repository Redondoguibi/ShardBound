#include "scripting/Runtime.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace game::script;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action,const std::string& code) {
    try { action(); } catch(const game::DiagnosticError& e) {
        if(e.diagnostic.code!=code) throw std::runtime_error("Expected "+code+", got "+e.what());
        return;
    }
    throw std::runtime_error("Expected diagnostic: "+code);
}
int main() {
    try {
        Runtime runtime; runtime.registerSymbol("Fire");
        runtime.bind("stub",native([](const auto& args,const auto&) { return args.at(0); }));
        runtime.execute("test",R"SL(
? language v0 executable contract
/? multiline comment
ignored ?/
func multiply(value, factor = 2):
    return value * factor
func pair():
    return 1, 2
@Globals
.LIMIT = 4
total = 0
for value in [1, 2, 3]:
    total = total + multiply(value)
if total == 12 and not false:
    total = stub(total + LIMIT)
else:
    total = 0
@Globals
result = total
pair_result = pair()
@Entities
enemy Zombie {
    Attributes:
        .health = 30
        .speed = 1.0
    AIs:
        .Melee
}
enemy FireZombie from Zombie {
    Attributes:
        .health = 50
        .element = Fire
}
@Events
on_tick(value):
    total = total + value
)SL","contract.sl");
        require(runtime.global("test","result").number()==16,"Functions/loops/defaults/precedence failed");
        const auto& zombie=runtime.registry().get("Entities","test:Zombie");
        const auto& fire=runtime.registry().get("Entities","test:FireZombie");
        require(zombie.data.member("Attributes",{}).member("health",{}).number()==30,"from mutated base");
        require(fire.data.member("Attributes",{}).member("speed",{}).number()==1,"from lost inherited property");
        require(runtime.emit("on_tick",{Value(3.0)}).empty(),"Event failed");
        require(runtime.global("test","total").number()==19,"Event did not update module global");
        runtime.execute("collections",R"SL(
@Globals
entries = get_definitions("Entities")
catalog_size = list_count(entries)
copied = list_at(entries, 0)
copied.data.Attributes.health = 999
values = []
for index in number_range(3):
    list_append(values, index)
last = list_at(values, 2)
present = definition_exists("Entities", "test:Zombie")
absent = definition_exists("Items", "missing:Item")
)SL");
        require(runtime.global("collections","catalog_size").number()==2 && runtime.global("collections","last").number()==2,"Collection enumeration failed");
        require(runtime.global("collections","present").boolean() && !runtime.global("collections","absent").boolean(),"Definition presence failed");
        require(runtime.registry().get("Entities","test:FireZombie").data.member("Attributes",{}).member("health",{}).number()==50,"Enumeration modified registry");
        rejects([&] { runtime.execute("collections","list_at(values, -1)"); },"SCRIPT301");
        rejects([&] { runtime.execute("collections","number_range(4097)"); },"SCRIPT301");
        rejects([&] { runtime.execute("collections","list_count(1)"); },"SCRIPT301");
        require(std::get<std::shared_ptr<List>>(runtime.global("test","pair_result").data)->values.size()==2,"Multiple returns failed");
        rejects([&] { runtime.execute("test","@Globals\nLIMIT = 5\n"); },"SCRIPT303");
        rejects([&] { runtime.execute("types","@Globals\nx = \"10\" + 5\n"); },"SCRIPT301");
        rejects([&] { runtime.execute("scope","if true:\n    hidden = 1\n@Globals\nx = hidden\n"); },"SCRIPT102");
        rejects([&] { runtime.execute("unknown","x = Firee\n"); },"SCRIPT102");
        rejects([&] { parse("func f(a = 1, b):\n    return b\n","bad.sl"); },"SCRIPT202");
        rejects([&] { parse("if true:\n \tx = 1\n","bad.sl"); },"SCRIPT101");
        std::string nested; for(int i=0;i<129;++i) nested+="entity Nested {\n";
        for(int i=0;i<129;++i) nested+="}\n";
        rejects([&] { parse(nested,"nested.sl"); },"SCRIPT201");
        runtime.execute("core","@Entities\nenemy player:\n    .health = 100\n    .speed = 1\n");
        runtime.execute("patch_a","@Core/Entities\nmodify player {\n    .health = 150\n}\n");
        runtime.execute("patch_b","@Core/Entities\nmodify player {\n    .speed = 2\n}\n");
        rejects([&] { runtime.execute("patch_c","@Core/Entities\nmodify player {\n    .health = 200\n}\n"); },"SCRIPT403");
        require(runtime.registry().get("Entities","core:player").data.member("health",{}).number()==150,"Failed patch changed target");
        std::filesystem::create_directories("user/script-tests");
        { std::ofstream out("user/script-tests/a.sl"); out<<"import \"b.sl\"\n"; }
        { std::ofstream out("user/script-tests/b.sl"); out<<"import \"a.sl\"\n"; }
        rejects([&] { runtime.load("cycle","user/script-tests","a.sl"); },"SCRIPT309");
        for(int i=0;i<65;++i) {
            std::ofstream output("user/script-tests/deep"+std::to_string(i)+".sl");
            if(i<64) output<<"import \"deep"<<i+1<<".sl\"\n"; else output<<"x = 1\n";
        }
        rejects([&] { runtime.load("deep","user/script-tests","deep0.sl"); },"SCRIPT321");
        rejects([&] { runtime.execute("recursion","func recurse():\n    return recurse()\nrecurse()\n"); },"SCRIPT311");
        { std::ofstream out("user/script-tests/library.sl"); out<<"private_value = 4\nfunc exported():\n    return private_value\n"; }
        { std::ofstream out("user/script-tests/main.sl"); out<<"import \"library.sl\"\n@Globals\nanswer = exported()\n"; }
        runtime.load("imports","user/script-tests","main.sl");
        require(runtime.global("imports","answer").number()==4,"Import closure failed");
        rejects([&] { runtime.execute("imports","x = private_value\n"); },"SCRIPT102");
        std::cout<<"Language v0 contract passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
