#pragma once
#include "scripting/Syntax.h"
#include <functional>
#include <map>
#include <variant>
namespace game::script {
struct Symbol { std::string name; auto operator<=>(const Symbol&) const = default; };
struct List;
struct Object;
struct Callable;
struct HostObject;
struct Value {
    using Storage=std::variant<std::monostate,double,bool,std::string,Symbol,std::shared_ptr<List>,std::shared_ptr<Object>,std::shared_ptr<Callable>,std::shared_ptr<HostObject>>;
    Storage data;
    Value()=default;
    explicit Value(double x) : data(x) {}
    explicit Value(bool x) : data(x) {}
    explicit Value(std::string x) : data(std::move(x)) {}
    explicit Value(Symbol x) : data(std::move(x)) {}
    explicit Value(std::shared_ptr<List> x) : data(std::move(x)) {}
    explicit Value(std::shared_ptr<Object> x) : data(std::move(x)) {}
    explicit Value(std::shared_ptr<Callable> x) : data(std::move(x)) {}
    explicit Value(std::shared_ptr<HostObject> x) : data(std::move(x)) {}
    double number(const Location& at={}) const;
    bool boolean(const Location& at={}) const;
    std::string string(const Location& at={}) const;
    Value member(const std::string& key,const Location& at) const;
    void setMember(const std::string& key,Value value,const Location& at);
};
struct List { std::vector<Value> values; };
struct Object { std::map<std::string,Value> fields; };
struct Callable { std::function<Value(const std::vector<Value>&,const Location&)> call; };
struct HostObject {
    virtual ~HostObject()=default;
    virtual Value get(const std::string& key,const Location& at)=0;
    virtual void set(const std::string& key,Value value,const Location& at)=0;
};
Value native(std::function<Value(const std::vector<Value>&,const Location&)> call);
Value list(std::vector<Value> values);
Value object();
Value clone(const Value& value);
bool equal(const Value& a,const Value& b);
}
