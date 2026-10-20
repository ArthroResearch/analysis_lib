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
        
        auto health = client_.Health();
        if (!health || health->status != "ok") {
            throw std::runtime_error("Daemon failed to respond to health check");
        }
        health_ = std::move(*health);
    }

    // The daemon's /health reply from start-up: its pid and, from contract
    // 2.0.0, the api_version it speaks. Worth logging, since people run their
    // own modified daemons.
    const HealthResponse& GetHealth() const { return health_; }

    AnalysisResponse Analyze(const AnalysisRequest& request) {
        return client_.Analyze(request);
    }

    // Ask the daemon to exit, and give it time to before it is killed.
    //
    // The wait is the point. Posting /shutdown and terminating in the next
    // statement -- which is what this used to do -- kills the daemon before it
    // can act on the request, because it serialises jobs and will not serve
    // /shutdown until any analysis in flight has finished. See
    // DaemonManager::kShutdownGraceMs.
    //
    // Safe to call twice: the POST to a daemon that has already gone just
    // fails, and Stop() does nothing once the handle is closed.
    void Shutdown() {
        client_.Shutdown();
        daemon_.Stop(DaemonManager::kShutdownGraceMs);
    }

    // Shutting down is what destruction MEANS here, so a caller that simply
    // drops this object gets the graceful path rather than a kill. Nothing may
    // escape a destructor: an unreachable daemon is already being killed by
    // ~DaemonManager, which is the outcome either way.
    ~NowAnalysis() {
        try {
            Shutdown();
        } catch (...) {
        }
    }

    NowAnalysis(const NowAnalysis&) = delete;
    NowAnalysis& operator=(const NowAnalysis&) = delete;

    Client& GetClient() { return client_; }
    DaemonManager& GetDaemonManager() { return daemon_; }

private:
    DaemonManager daemon_;
    Client client_;
    HealthResponse health_;
};

} // namespace now_analysis
