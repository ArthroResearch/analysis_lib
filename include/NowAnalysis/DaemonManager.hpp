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

    ~DaemonManager() {
        Stop();
    }

    // Explicit stop if needed
    void Stop() {
        if (hProcess_ != NULL) {
            // We should ideally send /shutdown via HTTP, but as a fallback we terminate
            // Since this is RAII, we want to be sure it's gone.
            // A better way is to call the Client::Shutdown() first if possible.
            TerminateProcess(hProcess_, 0);
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
