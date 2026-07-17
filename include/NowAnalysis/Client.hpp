#pragma once
#include "Types.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>

namespace now_analysis {

class Client {
public:
    Client(const std::string& host = "127.0.0.1", int port = 8765)
        : cli_(host, port) 
    {
        cli_.set_connection_timeout(5, 0); // 5 seconds
        cli_.set_read_timeout(60, 0);       // 60 seconds for report generation
    }

    bool IsHealthy() {
        if (auto res = cli_.Get("/health")) {
            if (res->status == 200) {
                auto j = nlohmann::json::parse(res->body);
                return j["status"] == "ok";
            }
        }
        return false;
    }

    AnalysisResponse Analyze(const AnalysisRequest& request) {
        nlohmann::json j = request;
        if (auto res = cli_.Post("/analyze", j.dump(), "application/json")) {
            if (res->status == 200 || res->status == 400 || res->status == 500) {
                return nlohmann::json::parse(res->body).get<AnalysisResponse>();
            } else {
                throw std::runtime_error("Unexpected HTTP status: " + std::to_string(res->status));
            }
        } else {
            throw std::runtime_error("Failed to connect to daemon");
        }
    }

    void Shutdown() {
        cli_.Post("/shutdown");
    }

private:
    httplib::Client cli_;
};

} // namespace now_analysis
