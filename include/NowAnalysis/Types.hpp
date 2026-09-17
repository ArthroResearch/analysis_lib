#pragma once
#include <string>
#include <vector>
#include <cstdint>
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

// to_number, not as_double: JSON has one number type, but Boost.JSON stores
// 203 as an integer and as_double() throws on it. open_viewer defaults to true
// when absent, as the daemon's does.
inline AnalysisRequest analysis_request_from_json(const boost::json::value& jv) {
    AnalysisRequest p;
    const auto& obj = jv.as_object();
    p.data_dir = obj.at("data_dir").as_string();
    p.type = obj.at("type").as_string();
    if (obj.contains("torque_target") && !obj.at("torque_target").is_null())
        p.torque_target = obj.at("torque_target").to_number<double>();
    if (obj.contains("opposite_angle") && !obj.at("opposite_angle").is_null())
        p.opposite_angle = obj.at("opposite_angle").to_number<double>();
    if (obj.contains("open_viewer"))
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
    std::string status;                  // "ok" | "partial" | "error"
    std::vector<AnalysisResultEntry> results;

    // Set only when the daemon replied with an error envelope rather than a
    // results payload. `error` is the daemon's code (invalid_json,
    // invalid_request, internal_error, not_found); `message` is its detail;
    // `traceback` is the daemon's Python traceback, sent with internal_error.
    std::optional<std::string> error;
    std::optional<std::string> message;
    std::optional<std::string> traceback;
};

// The daemon has TWO response shapes, and the HTTP status does not tell them
// apart: both can arrive as 500. A results payload is {"status", "results"},
// with status ok, partial or error. An error envelope is {"status": "fault",
// "error", "message"}. The contract is api/now_analysis.yaml in NowAnalysis.
//
// BOTH ENVELOPE SHAPES ARE ACCEPTED, PERMANENTLY. Before contract 2.0.0 the
// envelope had no status at all and was recognised by the missing key. People
// run their own modified daemons, so this client will meet older ones; keeping
// the old shape costs one condition and keeps their errors readable.
//
// An envelope comes back with status "error", whichever shape it arrived in, so
// callers test `status == "ok"` and then `error.has_value()` as they always
// have; `fault` never escapes this function.
//
// This once called obj.at("status") unconditionally, so every error envelope
// threw boost::json "out of range" and destroyed the diagnosis on its way past.
// Discriminate on the body, never on the HTTP status code.
inline AnalysisResponse analysis_response_from_json(const boost::json::value& jv) {
    AnalysisResponse p;
    const auto& obj = jv.as_object();

    const auto string_at = [&obj](const char* key) -> std::optional<std::string> {
        if (obj.contains(key) && obj.at(key).is_string())
            return std::string(obj.at(key).as_string());
        return std::nullopt;
    };

    if (!obj.contains("status") || string_at("status") == "fault") {
        p.status = "error";
        p.error = string_at("error");
        p.message = string_at("message");
        p.traceback = string_at("traceback");
        return p;
    }

    p.status = obj.at("status").as_string();

    // Absent on non-analysis replies such as {"status": "shutting_down"}.
    if (obj.contains("results") && obj.at("results").is_array())
        for (const auto& entry : obj.at("results").as_array())
            p.results.push_back(analysis_result_entry_from_json(entry));

    return p;
}

struct HealthResponse {
    std::string status;
    int pid = 0;
    // The contract version the daemon speaks. Absent from daemons older than
    // 2.0.0, which is a thing to log, not a reason to refuse the daemon.
    std::optional<std::string> api_version;
};

inline HealthResponse health_response_from_json(const boost::json::value& jv) {
    HealthResponse p;
    const auto& obj = jv.as_object();
    p.status = obj.at("status").as_string();
    p.pid = static_cast<int>(obj.at("pid").to_number<std::int64_t>());
    if (obj.contains("api_version") && obj.at("api_version").is_string())
        p.api_version = std::string(obj.at("api_version").as_string());
    return p;
}

} // namespace now_analysis
