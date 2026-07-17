#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace now_analysis {

struct AnalysisRequest {
    std::string data_dir;
    std::string type;
    std::optional<double> torque_target;
    std::optional<double> opposite_angle;
    bool open_viewer = true;
};

inline void to_json(nlohmann::json& j, const AnalysisRequest& p) {
    j = nlohmann::json{
        {"data_dir", p.data_dir},
        {"type", p.type},
        {"torque_target", p.torque_target.has_value() ? nlohmann::json(*p.torque_target) : nlohmann::json()},
        {"opposite_angle", p.opposite_angle.has_value() ? nlohmann::json(*p.opposite_angle) : nlohmann::json()},
        {"open_viewer", p.open_viewer}
    };
}

inline void from_json(const nlohmann::json& j, AnalysisRequest& p) {
    j.at("data_dir").get_to(p.data_dir);
    j.at("type").get_to(p.type);
    if (j.contains("torque_target") && !j["torque_target"].is_null()) p.torque_target = j["torque_target"].get<double>();
    if (j.contains("opposite_angle") && !j["opposite_angle"].is_null()) p.opposite_angle = j["opposite_angle"].get<double>();
    j.at("open_viewer").get_to(p.open_viewer);
}

struct AnalysisResultEntry {
    std::string type;
    std::optional<std::string> pdf;
    bool generated = false;
    std::optional<std::string> generation_error;
    bool viewer_launched = false;
    std::optional<std::string> viewer_error;
};

inline void to_json(nlohmann::json& j, const AnalysisResultEntry& p) {
    j = nlohmann::json{
        {"type", p.type},
        {"pdf", p.pdf.has_value() ? nlohmann::json(*p.pdf) : nlohmann::json()},
        {"generated", p.generated},
        {"generation_error", p.generation_error.has_value() ? nlohmann::json(*p.generation_error) : nlohmann::json()},
        {"viewer_launched", p.viewer_launched},
        {"viewer_error", p.viewer_error.has_value() ? nlohmann::json(*p.viewer_error) : nlohmann::json()}
    };
}

inline void from_json(const nlohmann::json& j, AnalysisResultEntry& p) {
    j.at("type").get_to(p.type);
    if (j.contains("pdf") && !j["pdf"].is_null()) p.pdf = j["pdf"].get<std::string>();
    j.at("generated").get_to(p.generated);
    if (j.contains("generation_error") && !j["generation_error"].is_null()) p.generation_error = j["generation_error"].get<std::string>();
    j.at("viewer_launched").get_to(p.viewer_launched);
    if (j.contains("viewer_error") && !j["viewer_error"].is_null()) p.viewer_error = j["viewer_error"].get<std::string>();
}

struct AnalysisResponse {
    std::string status;
    std::vector<AnalysisResultEntry> results;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AnalysisResponse, status, results)

struct HealthResponse {
    std::string status;
    int pid;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HealthResponse, status, pid)

} // namespace now_analysis
