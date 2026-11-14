#include "tools/EditorProcess.h"
#include <windows.h>
#include <array>
#include <algorithm>
#include <cstddef>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace game {
namespace {
struct Close {void operator()(void* handle) const {CloseHandle(handle);} };
using Handle=std::unique_ptr<void,Close>;
void require(BOOL success,const char* action) {
    if(!success) throw std::runtime_error(std::string(action)+" ("+std::to_string(GetLastError())+")");
}
std::wstring quote(const std::wstring& arg) {
    std::wstring result=L"\"";size_t slashes=0;
    for(const auto c:arg) {
        if(c==L'\\') {++slashes;continue;}
        result.append(c==L'\"'?2*slashes+1:slashes,L'\\');slashes=0;result+=c;
    }
    result.append(2*slashes,L'\\');return result+L'\"';
}
}
EditorProcessResult runEditorProcess(const std::wstring& program,const std::vector<std::wstring>& args,
    const std::filesystem::path& directory,std::string input,std::stop_token stop) {
    if(stop.stop_requested()) throw std::runtime_error("Operação cancelada");
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    HANDLE read{},write{};
    require(CreatePipe(&read,&write,&sa,0),"Cannot open process output");
    Handle outRead(read),outWrite(write);
    require(SetHandleInformation(read,HANDLE_FLAG_INHERIT,0),"Cannot protect process output handle");
    require(CreatePipe(&read,&write,&sa,0),"Cannot open process input");
    Handle inRead(read),inWrite(write);
    require(SetHandleInformation(write,HANDLE_FLAG_INHERIT,0),"Cannot protect process input handle");
    Handle job(CreateJobObjectW(nullptr,nullptr));require(bool(job),"Cannot create process job");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    require(SetInformationJobObject(job.get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"Cannot configure process job");
    auto command=quote(program);for(const auto& arg:args) command+=L" "+quote(arg);
    STARTUPINFOEXW start{};start.StartupInfo.cb=sizeof(start);start.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    start.StartupInfo.hStdInput=inRead.get();start.StartupInfo.hStdOutput=outWrite.get();start.StartupInfo.hStdError=outWrite.get();
    SIZE_T attributesSize=0;InitializeProcThreadAttributeList(nullptr,1,0,&attributesSize);
    std::vector<std::byte> attributes(attributesSize);
    start.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    require(InitializeProcThreadAttributeList(start.lpAttributeList,1,0,&attributesSize),"Cannot initialize process attributes");
    const std::unique_ptr<std::remove_pointer_t<LPPROC_THREAD_ATTRIBUTE_LIST>,decltype(&DeleteProcThreadAttributeList)> attributeGuard(
        start.lpAttributeList,DeleteProcThreadAttributeList);
    std::array<HANDLE,2> inherited{inRead.get(),outWrite.get()};
    require(UpdateProcThreadAttribute(start.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited.data(),sizeof(inherited),nullptr,nullptr),
        "Cannot restrict process handle inheritance");
    PROCESS_INFORMATION process{};
    require(CreateProcessW(nullptr,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,
        directory.c_str(),&start.StartupInfo,&process),"Cannot start process");
    Handle child(process.hProcess),thread(process.hThread);
    if(!AssignProcessToJobObject(job.get(),child.get())) {TerminateProcess(child.get(),1);require(FALSE,"Cannot attach process to job");}
    require(ResumeThread(thread.get())!=DWORD(-1),"Cannot resume process");
    inRead.reset();outWrite.reset();
    std::jthread writer([pipe=std::move(inWrite),data=std::move(input)]() mutable {
        for(size_t sent=0;sent<data.size();) {
            DWORD written=0;
            if(!WriteFile(pipe.get(),data.data()+sent,DWORD(std::min<size_t>(data.size()-sent,8192)),&written,nullptr) || !written) break;
            sent+=written;
        }
        if(!data.empty()) SecureZeroMemory(data.data(),data.size());
    });
    std::string output;
    std::array<char,8192> buffer{};
    const auto drain=[&] {
        DWORD available=0;
        while(!stop.stop_requested() && PeekNamedPipe(outRead.get(),nullptr,0,nullptr,&available,nullptr) && available) {
            DWORD got=0;
            if(!ReadFile(outRead.get(),buffer.data(),std::min(available,DWORD(buffer.size())),&got,nullptr) || !got) break;
            if(output.size()<1024*1024) output.append(buffer.data(),got);
        }
    };
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
    bool timedOut=false;
    DWORD wait=WAIT_TIMEOUT;
    while((wait=WaitForSingleObject(child.get(),35))==WAIT_TIMEOUT) {
        if(stop.stop_requested()) break;
        if(std::chrono::steady_clock::now()>=deadline) {timedOut=true;break;}
        drain();
    }
    // Release inherited pipe handles before joining the writer, including when a child spawned another process.
    TerminateJobObject(job.get(),1);
    if(stop.stop_requested()) throw std::runtime_error("Operação cancelada");
    if(timedOut) throw std::runtime_error("O processo excedeu o limite de dez minutos");
    require(wait==WAIT_OBJECT_0,"Cannot wait for process");
    drain();DWORD exitCode=0;require(GetExitCodeProcess(child.get(),&exitCode),"Cannot read exit code");
    return {exitCode,std::move(output)};
}
}
