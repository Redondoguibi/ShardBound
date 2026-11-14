#include "core/AtomicFile.h"
#include <Windows.h>
#include <atomic>
#include <fstream>
#include <thread>
namespace game {
void renameDirectory(const std::filesystem::path& source,const std::filesystem::path& destination) {
    for(unsigned attempt=0;;++attempt) {
        if(MoveFileExW(source.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)) return;
        const auto error=GetLastError();
        if(attempt==20 || (error!=ERROR_SHARING_VIOLATION && error!=ERROR_LOCK_VIOLATION && error!=ERROR_ACCESS_DENIED) || std::filesystem::exists(destination))
            throw std::filesystem::filesystem_error("Cannot rename directory",source,destination,std::error_code(int(error),std::system_category()));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
void flushFile(const std::filesystem::path& path) {
    const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot flush file: "+path.string());
    const auto ok=FlushFileBuffers(file);const auto error=GetLastError();CloseHandle(file);
    if(!ok) throw std::runtime_error("File flush failed: "+std::to_string(error));
}
void publishFile(const std::filesystem::path& temporary,const std::filesystem::path& destination,bool backup) {
    for(unsigned attempt=0;;++attempt) {
        BOOL ok;
        if(backup && std::filesystem::exists(destination)) ok=ReplaceFileW(destination.c_str(),temporary.c_str(),(destination.wstring()+L".bak").c_str(),0,nullptr,nullptr);
        else ok=MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        if(ok) return;const auto error=GetLastError();
        if(attempt==4 || (error!=ERROR_SHARING_VIOLATION && error!=ERROR_LOCK_VIOLATION)) throw std::runtime_error("Cannot publish file: "+destination.string()+" ("+std::to_string(error)+")");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
void atomicWrite(const std::filesystem::path& destination,std::string_view data,bool backup,const std::function<void(WriteStage)>& observer) {
    static std::atomic<std::uint64_t> serial=0;
    std::filesystem::create_directories(destination.parent_path());
    const auto temp=destination.wstring()+L".pending-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial);
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(data.data(),std::streamsize(data.size()));out.close();if(!out) throw std::runtime_error("Cannot write file: "+destination.string());}
    flushFile(temp);if(observer) observer(WriteStage::Flushed);
    publishFile(temp,destination,backup);if(observer) observer(WriteStage::Published);
}
}
