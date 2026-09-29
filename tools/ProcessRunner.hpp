#pragma once
// A hard timeout belongs outside the parser. Each input gets an OS process.
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace nextgen::tools {
struct ProcessResult { int exitCode = -1; bool timedOut = false; std::string error; };
inline ProcessResult RunProcess(const std::filesystem::path& executable,
    const std::vector<std::filesystem::path>& arguments, const std::filesystem::path& output,
    std::chrono::milliseconds timeout) {
#ifdef _WIN32
    struct Handle {
        HANDLE value = nullptr;
        ~Handle() { if(value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    };
    const auto quote = [](const std::wstring& text) {
        std::wstring out = L"\""; std::size_t slashes = 0;
        for(wchar_t ch : text) {
            if(ch == L'\\') { ++slashes; continue; }
            out.append(slashes * (ch == L'"' ? 2 : 1), L'\\'); slashes = 0;
            if(ch == L'"') out += L'\\';
            out += ch;
        }
        out.append(slashes * 2, L'\\'); out += L'"'; return out;
    };
    auto command = quote(executable.wstring());
    for(const auto& arg : arguments) command += L" " + quote(arg.wstring());
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    Handle log{CreateFileW(output.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(log.value == INVALID_HANDLE_VALUE) return {-1,false,"Cannot open child output"};
    Handle job{CreateJobObjectW(nullptr,nullptr)};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.value || !SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
        return {-1,false,"Cannot configure child process job"};
    // In a multithreaded scanner, inheriting every inheritable handle also
    // inherits other workers' logs and keeps their files locked. Whitelist only
    // this child's output handle.
    SIZE_T bytes=0;
    InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<std::byte> attributeStorage(bytes);
    auto* attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if(!InitializeProcThreadAttributeList(attributes,1,0,&bytes))return {-1,false,"Cannot initialize child handle list"};
    struct Attributes { LPPROC_THREAD_ATTRIBUTE_LIST value; ~Attributes(){DeleteProcThreadAttributeList(value);} } cleanup{attributes};
    if(!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,&log.value,sizeof(HANDLE),nullptr,nullptr))
        return {-1,false,"Cannot configure child handle inheritance"};
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb=sizeof(startup); startup.lpAttributeList=attributes;
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput=log.value; startup.StartupInfo.hStdError=log.value; startup.StartupInfo.hStdInput=nullptr;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,
            CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&process))
        return {-1,false,"Cannot launch child: "+std::to_string(GetLastError())};
    Handle child{process.hProcess}, thread{process.hThread};
    if(!AssignProcessToJobObject(job.value,child.value)) {
        TerminateProcess(child.value,1); WaitForSingleObject(child.value,INFINITE);
        return {-1,false,"Cannot contain child process"};
    }
    if(ResumeThread(thread.value)==DWORD(-1)) {
        TerminateJobObject(job.value,1); WaitForSingleObject(child.value,INFINITE);
        return {-1,false,"Cannot start child process"};
    }
    const auto wait=WaitForSingleObject(child.value,static_cast<DWORD>(timeout.count()));
    if(wait!=WAIT_OBJECT_0) {
        TerminateJobObject(job.value,1); WaitForSingleObject(child.value,INFINITE);
        return {-1,wait==WAIT_TIMEOUT,wait==WAIT_TIMEOUT ? "" : "Process wait failed"};
    }
    DWORD code=0; GetExitCodeProcess(child.value,&code);
    return {static_cast<int>(code),false,{}};
#else
    // Prepare all allocations before fork: callers may run multiple workers.
    std::vector<std::string> strings{executable.string()};
    for(const auto& arg:arguments) strings.push_back(arg.string());
    std::vector<char*> args;
    for(auto& str:strings) args.push_back(str.data());
    args.push_back(nullptr);
    const int log=open(output.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
    if(log<0) return {-1,false,"Cannot open child output"};
    const pid_t child=fork();
    if(child==0) {
        setpgid(0,0); dup2(log,STDOUT_FILENO); dup2(log,STDERR_FILENO); close(log);
        execv(executable.c_str(),args.data()); _exit(127);
    }
    close(log);
    if(child<0) return {-1,false,"Cannot fork child"};
    setpgid(child,child);
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    int status=0;
    while(true) {
        const auto result=waitpid(child,&status,WNOHANG);
        if(result==child) return {WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status),false,{}};
        if(result<0 && errno!=EINTR) return {-1,false,"Cannot wait for child"};
        if(std::chrono::steady_clock::now()>=deadline) {
            kill(-child,SIGKILL); kill(child,SIGKILL);
            while(waitpid(child,&status,0)<0 && errno==EINTR) {}
            return {-1,true,{}};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#endif
}
} // namespace nextgen::tools
