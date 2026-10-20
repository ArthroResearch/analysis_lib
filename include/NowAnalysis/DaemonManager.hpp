#pragma once
#include <windows.h>
#include <string>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <format>

namespace now_analysis {

class DaemonManager {
public:
    DaemonManager(const std::wstring& pythonExecutable, 
                  const std::wstring& scriptPath, 
                  const std::string& host = "127.0.0.1", 
                  int port = 8765)
        : host_(host), port_(port) 
    {
        std::wstring commandLine = std::format(L"\"{}\" \"{}\" --daemon --host {} --port {}", 
                                               pythonExecutable, scriptPath, 
                                               std::wstring(host.begin(), host.end()), port);
        
        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // Run in background

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        // Create the process with a pipe to read its output and check if it's ready
        // For simplicity in this example, we'll just start it and let the user wait or check health.
        // The README says it prints "NOW_ANALYSIS_DAEMON_READY" when ready.
        
        if (!CreateProcessW(NULL, const_cast<LPWSTR>(commandLine.c_str()), NULL, NULL, FALSE, 
                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            throw std::runtime_error("Failed to launch daemon process. Error: " + std::to_string(GetLastError()));
        }

        hProcess_ = pi.hProcess;
        hThread_ = pi.hThread;
        pid_ = pi.dwProcessId;

        // Give it a moment to start up
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // How long Stop() waits for the daemon to go on its own before killing it.
    //
    // THE DAEMON SERIALISES JOBS. A /shutdown arriving during an analysis is not
    // served until that analysis finishes, so terminating the moment after
    // posting it kills the daemon mid-write and leaves a half-finished PDF or
    // model JSON. That was survivable while /analyze took about a second; POST
    // /model takes ~12 s, which makes the window wide enough to hit.
    //
    // Fifteen seconds covers a model run with room to spare. Past that the
    // daemon is wedged rather than busy, and the kill below is the right answer
    // -- this is a bound on "never comes back", not a performance target.
    static constexpr DWORD kShutdownGraceMs = 15000;

    ~DaemonManager() {
        Stop();
    }

    // Stop the daemon, giving it `graceMs` to exit by itself first.
    //
    // The default is zero because the destructor reaches here having asked the
    // daemon nothing: waiting for an exit nobody requested would just hang the
    // caller. NowAnalysis::Shutdown posts /shutdown and then passes the grace,
    // which is the path that wants one.
    void Stop(DWORD graceMs = 0) {
        if (hProcess_ != NULL) {
            // WAIT_OBJECT_0 means it exited on its own and needs no killing.
            // Anything else -- timeout, or the wait itself failing -- falls
            // through to the terminate, because this is RAII and the process
            // must be gone when we return.
            const bool exited =
                graceMs > 0 && WaitForSingleObject(hProcess_, graceMs) == WAIT_OBJECT_0;
            if (!exited) {
                TerminateProcess(hProcess_, 0);
            }
            CloseHandle(hProcess_);
            CloseHandle(hThread_);
            hProcess_ = NULL;
            hThread_ = NULL;
        }
    }

    DWORD GetPid() const { return pid_; }

private:
    HANDLE hProcess_ = NULL;
    HANDLE hThread_ = NULL;
    DWORD pid_ = 0;
    std::string host_;
    int port_;
};

} // namespace now_analysis
