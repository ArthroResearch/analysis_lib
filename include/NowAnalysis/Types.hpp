#pragma once
#include <string>
#include <vector>
#include <optional>
#include <boost/json.hpp>

namespace now_analysis {

struct AnalysisRequest {
    std::string data_dir;
    std::string type;
    std::optional<double> torque_target;
    std::optional<double> opposite_angle;
    bool open_viewer = true;
};

inline boost::json::value to_json(const AnalysisRequest& p) {
    boost::json::object obj;
    obj["data_dir"] = p.data_dir;
    obj["type"] = p.type;
    if (p.torque_target.has_value())
        obj["torque_target"] = *p.torque_target;
    else
        obj["torque_target"] = nullptr;
    if (p.opposite_angle.has_value())
        obj["opposite_angle"] = *p.opposite_angle;
    else
        obj["opposite_angle"] = nullptr;
    obj["open_viewer"] = p.open_viewer;
    return obj;
}

inline AnalysisRequest analysis_request_from_json(const boost::json::value& jv) {
    AnalysisRequest p;
    const auto& obj = jv.as_object();
    p.data_dir = obj.at("data_dir").as_string();
    p.type = obj.at("type").as_string();
    if (obj.contains("torque_target") && !obj.at("torque_target").is_null())
        p.torque_target = obj.at("torque_target").as_double();
    if (obj.contains("opposite_angle") && !obj.at("opposite_angle").is_null())
        p.opposite_angle = obj.at("opposite_angle").as_double();
    p.open_viewer = obj.at("open_viewer").as_bool();
    return p;
}

struct AnalysisResultEntry {
    std::string type;
    std::optional<std::string> pdf;
    bool generated = false;
    std::optional<std::string> generation_error;
    bool viewer_launched = false;
    std::optional<std::string> viewer_error;
};

inline AnalysisResultEntry analysis_result_entry_from_json(const boost::json::value& jv) {
    AnalysisResultEntry p;
    const auto& obj = jv.as_object();
    p.type = obj.at("type").as_string();
    if (obj.contains("pdf") && !obj.at("pdf").is_null())
        p.pdf = std::string(obj.at("pdf").as_string());
    p.generated = obj.at("generated").as_bool();
    if (obj.contains("generation_error") && !obj.at("generation_error").is_null())
        p.generation_error = std::string(obj.at("generation_error").as_string());
    p.viewer_launched = obj.at("viewer_launched").as_bool();
    if (obj.contains("viewer_error") && !obj.at("viewer_error").is_null())
        p.viewer_error = std::string(obj.at("viewer_error").as_string());
    return p;
}

struct AnalysisResponse {
    std::string status;
    std::vector<AnalysisResultEntry> results;
};

inline AnalysisResponse analysis_response_from_json(const boost::json::value& jv) {
    AnalysisResponse p;
    const auto& obj = jv.as_object();
    p.status = obj.at("status").as_string();
    for (const auto& entry : obj.at("results").as_array())
        p.results.push_back(analysis_result_entry_from_json(entry));
    return p;
}

struct HealthResponse {
    std::string status;
    int pid;
};

inline HealthResponse health_response_from_json(const boost::json::value& jv) {
    HealthResponse p;
    const auto& obj = jv.as_object();
    p.status = obj.at("status").as_string();
    p.pid = static_cast<int>(obj.at("pid").as_int64());
    return p;
}

} // namespace now_analysis
