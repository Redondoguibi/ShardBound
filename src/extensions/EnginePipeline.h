#pragma once
#include "core/Data.h"
#include <functional>
#include <map>
#include <optional>
#include <deque>
#include <exception>
#include <cstdint>
namespace game {
// Session-owned extension points. No engine pointers cross the mod boundary.
class EnginePipeline {
public:
    using Callback=std::function<bool(const Data&)>;
    using Default=std::function<void(const Data&)>;
    struct Options {std::string mode="before",replaces;int api=1,priority=0;};
    std::function<bool(const std::string&,const std::string&)> depends;
    void add(std::string owner,std::string id,std::string stage,Callback callback,Options options);
    void remove(const std::string& owner,const std::string& id);
    void enabled(const std::string& owner,const std::string& id,bool value);
    void run(const std::string& stage,Data::Object parameters,const Default& implementation,const Default& validate={});
    void runDefault(const std::string& owner,const Data::Object& overrides={});
    Data inspect() const;
    static const std::vector<std::string>& stages();
private:
    struct Entry {std::string owner,id,stage;Callback callback;Options options;bool enabled=true;std::uint64_t generation=0,calls=0,errors=0;double last=0,peak=0,total=0;};
    struct Frame {std::string stage,owner,mode;Data::Object parameters;Default implementation,validate;bool started=false;std::exception_ptr failure;};
    std::map<std::string,Entry> entries_;
    std::map<std::string,std::vector<std::string>> replacements_;
    std::uint64_t sequence_=0;
    Frame* frame_=nullptr;
    std::deque<Data> diagnostics_;
    Entry& owned(const std::string& owner,const std::string& id);
    void invokeDefault(Frame& frame,const Data::Object& overrides);
};
}
