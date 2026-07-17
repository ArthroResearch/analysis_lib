#pragma once

#include "Types.hpp"
#include "Client.hpp"
#include "DaemonManager.hpp"

namespace now_analysis {

/**
 * @brief RAII wrapper that combines DaemonManager and Client.
 * 
 * This class ensures the daemon is started on construction and 
 * stopped on destruction. It provides methods to interact with the daemon.
 */
class NowAnalysis {
public:
    NowAnalysis(const std::wstring& pythonExecutable, 
                const std::wstring& scriptPath, 
                const std::string& host = "127.0.0.1", 
                int port = 8765)
        : daemon_(pythonExecutable, scriptPath, host, port),
          client_(host, port)
    {
        // Wait for daemon to be healthy
        int retries = 10;
        while (retries-- > 0 && !client_.IsHealthy()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        if (!client_.IsHealthy()) {
            throw std::runtime_error("Daemon failed to respond to health check");
        }
    }

    AnalysisResponse Analyze(const AnalysisRequest& request) {
        return client_.Analyze(request);
    }

    void Shutdown() {
        client_.Shutdown();
        daemon_.Stop();
    }

    Client& GetClient() { return client_; }
    DaemonManager& GetDaemonManager() { return daemon_; }

private:
    DaemonManager daemon_;
    Client client_;
};

} // namespace now_analysis
