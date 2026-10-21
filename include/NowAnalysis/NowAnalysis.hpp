#pragma once

#include "Types.hpp"
#include "Client.hpp"
#include "DaemonManager.hpp"

#include <chrono>
#include <string>

namespace now_analysis {

/**
 * @brief RAII wrapper that combines DaemonManager and Client.
 * 
 * This class ensures the daemon is started on construction and 
 * stopped on destruction. It provides methods to interact with the daemon.
 */
class NowAnalysis {
public:
    // How long the daemon is given to come up and answer /health.
    //
    // Thirty seconds BOUNDS "never came up" rather than targeting a startup
    // time. The measured figure is ~5 s; a cold filesystem or a slower machine
    // can take several times that, and the cost of being generous is paid only
    // when something is already broken.
    static constexpr std::chrono::seconds kHealthTimeout{30};
    static constexpr std::chrono::milliseconds kHealthPoll{200};

    NowAnalysis(const std::wstring& pythonExecutable, 
                const std::wstring& scriptPath, 
                const std::string& host = "127.0.0.1", 
                int port = 8765)
        : daemon_(pythonExecutable, scriptPath, host, port),
          client_(host, port)
    {
        // WAIT ON A DEADLINE, NOT A RETRY COUNT.
        //
        // This was 10 retries at 200 ms -- about 2.5 s including the launcher's
        // own initial sleep. That was enough for the single-file daemon, which
        // imported matplotlib and pandas. The ported one also imports scipy,
        // numpy and nine analysis modules, ALL AT MODULE TOP, and measures ~5 s
        // to ready on the Windows development machine (2026-10-21). It could
        // not answer inside the old budget, and every start failed.
        const auto started = std::chrono::steady_clock::now();
        const auto elapsed = [started] {
            return std::chrono::steady_clock::now() - started;
        };
        while (!client_.IsHealthy() && elapsed() < kHealthTimeout) {
            std::this_thread::sleep_for(kHealthPoll);
        }
        startup_ = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed());

        auto health = client_.Health();
        if (!health || health->status != "ok") {
            // Say how long we waited: without it, a slow start and a daemon
            // that died on import are the same message. They still look alike
            // -- the daemon is launched with no pipe, so its stderr goes
            // nowhere -- which is why the advice is here rather than in a
            // comment nobody reads at three in the morning.
            throw std::runtime_error(
                "Daemon failed to respond to health check after "
                + std::to_string(startup_.count())
                + " ms. Its output is not captured, so a crash on import is "
                  "invisible from here: run the daemon by hand to see why.");
        }
        health_ = std::move(*health);
    }

    // How long the daemon took to answer /health. Worth logging: it is the
    // measurement that sizes kHealthTimeout, and it grows every time a module
    // is added to the daemon's startup imports.
    std::chrono::milliseconds GetStartupDuration() const { return startup_; }

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
    std::chrono::milliseconds startup_{0};
};

} // namespace now_analysis
