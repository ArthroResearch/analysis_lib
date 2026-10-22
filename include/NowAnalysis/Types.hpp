#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <variant>
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

// A string field, or nullopt when absent or not a string.
//
// Guarding on contains() is not defensiveness: obj.at() on a missing key throws
// boost::json "out of range", and a throw while parsing an ERROR reply destroys
// the diagnosis on its way past -- which is exactly what happened here once,
// recorded below. Only fields the contract marks required may be read with a
// bare at().
inline std::optional<std::string> json_string_at(const boost::json::object& obj, const char* key) {
    if (obj.contains(key) && obj.at(key).is_string())
        return std::string(obj.at(key).as_string());
    return std::nullopt;
}

// True when this body is an error envelope rather than a results payload.
//
// ONE COPY OF THIS RULE, used by every endpoint's parser. /analyze and /model
// both have the two shapes and both discriminate the same way; a second copy
// would be a second chance to get it wrong, and the older no-status shape below
// is the kind of detail that gets fixed in one copy and not the other.
inline bool is_error_envelope(const boost::json::object& obj) {
    return !obj.contains("status") || json_string_at(obj, "status") == "fault";
}

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

    if (is_error_envelope(obj)) {
        p.status = "error";
        p.error = json_string_at(obj, "error");
        p.message = json_string_at(obj, "message");
        p.traceback = json_string_at(obj, "traceback");
        return p;
    }

    p.status = obj.at("status").as_string();

    // Absent on non-analysis replies such as {"status": "shutting_down"}.
    if (obj.contains("results") && obj.at("results").is_array())
        for (const auto& entry : obj.at("results").as_array())
            p.results.push_back(analysis_result_entry_from_json(entry));

    return p;
}

// ---------------------------------------------------------------------------
// /model -- the exam-level call. Contract: api/now_analysis.yaml, ModelRequest.
//
// /analyze AND /model ARE ALWAYS CALLED SEPARATELY, by design: /analyze runs
// once per TEST as each capture finishes and gives the operator that test's
// PDF; /model runs once per EXAM, fits the models and draws the exam-level
// reports. Neither is redundant with the other, and that is why the defaults
// here are every test and both legs -- the call is made once and means the
// whole exam.
// ---------------------------------------------------------------------------

struct ModelRequest {
    // The exam directory holding <test>kin.csv and <test>torque.csv. Required.
    std::string data_dir;

    // EMPTY MEANS ABSENT, which the daemon reads as every test / both legs.
    // Values: tests "ap", "rot", "vv"; sides "right", "left".
    std::vector<std::string> tests;
    std::vector<std::string> sides;

    // Absent leaves the daemon's own default in force rather than restating it
    // here -- a default written down twice is a default that drifts. The
    // daemon's are cycle 2 (the one the STAR report uses) and features true.
    std::optional<int> cycle;
    std::optional<bool> features;

    // Extra PDFs to draw beside the models: "star", "hysteresis", "cycles",
    // "features". Empty draws none. Order is honoured, and each is opened as
    // it is written, so ["star","features"] puts the star in front of the
    // operator before the legacy PDFs are drawn.
    std::vector<std::string> reports;

    // WHICH DRAWN REPORTS OPEN IN THE OS VIEWER. Five states, all meaningful,
    // which is why this is an optional variant rather than a bool:
    //
    //   nullopt            the default -- the STAR REPORT ALONE
    //   vector{"star",...} exactly those kinds
    //   vector{}           nothing opens
    //   true               every kind drawn (what 2.1.0 meant, still accepted)
    //   false              nothing opens
    //
    // Leave it nullopt unless you mean otherwise: the default is already what
    // a once-per-exam call wants, and three windows at once serves nobody.
    //
    // Selection is by report KIND, not by file. "features" draws two PDFs, the
    // region and the cycle reports, and this cannot separate them.
    std::optional<std::variant<bool, std::vector<std::string>>> open_viewer;
};

// Fields are omitted rather than sent as null. The contract types them (cycle
// is an integer, tests an array of enums) and sets additionalProperties false,
// so an explicit null is a schema violation where absence is the documented way
// to ask for the default. This differs from AnalysisRequest above, whose
// nullable numbers the contract does accept as null.
inline boost::json::value to_json(const ModelRequest& p) {
    boost::json::object obj;
    obj["data_dir"] = p.data_dir;

    const auto put_strings = [&obj](const char* key, const std::vector<std::string>& values) {
        if (values.empty())
            return;
        boost::json::array arr;
        for (const auto& value : values)
            arr.push_back(boost::json::value(value));
        obj[key] = std::move(arr);
    };

    put_strings("tests", p.tests);
    put_strings("sides", p.sides);
    put_strings("reports", p.reports);

    if (p.cycle.has_value())
        obj["cycle"] = *p.cycle;
    if (p.features.has_value())
        obj["features"] = *p.features;

    // An engaged optional holding an EMPTY vector is not the same as nullopt:
    // it serialises as [] and means "open nothing", where nullopt means "your
    // default". put_strings cannot be reused here for exactly that reason.
    if (p.open_viewer.has_value()) {
        if (const auto* flag = std::get_if<bool>(&*p.open_viewer)) {
            obj["open_viewer"] = *flag;
        } else {
            boost::json::array arr;
            for (const auto& kind : std::get<std::vector<std::string>>(*p.open_viewer))
                arr.push_back(boost::json::value(kind));
            obj["open_viewer"] = std::move(arr);
        }
    }

    return obj;
}

struct ModelResultEntry {
    std::string test;                   // "ap" | "rot" | "vv"
    std::string side;                   // "right" | "left"
    bool fitted = false;
    std::optional<std::string> model;   // path to the written JSON, or none
    std::optional<int> branches;        // half-cycles fitted; only when fitted
    // WHY A TEST CAN FAIL AND STILL BE HERE: a test that cannot be fitted is
    // returned with its reason rather than dropped. The C++ dropped it
    // silently, which is the behaviour the port exists to stop.
    std::optional<std::string> error;
};

inline ModelResultEntry model_result_entry_from_json(const boost::json::value& jv) {
    ModelResultEntry p;
    const auto& obj = jv.as_object();
    p.test = obj.at("test").as_string();
    p.side = obj.at("side").as_string();
    p.fitted = obj.at("fitted").as_bool();
    if (obj.contains("model") && !obj.at("model").is_null())
        p.model = std::string(obj.at("model").as_string());
    if (obj.contains("branches") && obj.at("branches").is_int64())
        p.branches = static_cast<int>(obj.at("branches").to_number<std::int64_t>());
    p.error = json_string_at(obj, "error");
    return p;
}

struct ModelReportEntry {
    std::string kind;                   // "star" | "hysteresis" | "cycles" | "features"
    std::optional<std::string> pdf;
    bool generated = false;
    bool viewer_launched = false;
    std::optional<std::string> viewer_error;
    std::optional<std::string> error;
};

inline ModelReportEntry model_report_entry_from_json(const boost::json::value& jv) {
    ModelReportEntry p;
    const auto& obj = jv.as_object();
    p.kind = obj.at("kind").as_string();
    if (obj.contains("pdf") && !obj.at("pdf").is_null())
        p.pdf = std::string(obj.at("pdf").as_string());
    p.generated = obj.at("generated").as_bool();
    // NOT REQUIRED BY THE CONTRACT, unlike AnalysisResultEntry's field of the
    // same name: a report nobody asked to open carries no viewer keys at all.
    // Reading this with a bare at() is the throw-while-parsing bug again.
    if (obj.contains("viewer_launched") && obj.at("viewer_launched").is_bool())
        p.viewer_launched = obj.at("viewer_launched").as_bool();
    p.viewer_error = json_string_at(obj, "viewer_error");
    p.error = json_string_at(obj, "error");
    return p;
}

struct ModelResponse {
    // "ok" | "error". Error when any requested test failed to fit or any
    // report failed to draw -- so results and reports are still worth reading
    // on an error, and the per-entry `error` says which one it was.
    std::string status;

    // The exam's UUID from metadata.txt. None if it has none.
    std::optional<std::string> exam_id;

    std::vector<ModelResultEntry> results;
    std::vector<ModelReportEntry> reports;

    // Set only for an error envelope, exactly as on AnalysisResponse: `error`
    // is the daemon's code, `message` its detail, `traceback` its Python
    // traceback on internal_error. A caller tests status == "ok" and then
    // error.has_value(); "fault" never escapes this function.
    std::optional<std::string> error;
    std::optional<std::string> message;
    std::optional<std::string> traceback;
};

inline ModelResponse model_response_from_json(const boost::json::value& jv) {
    ModelResponse p;
    const auto& obj = jv.as_object();

    if (is_error_envelope(obj)) {
        p.status = "error";
        p.error = json_string_at(obj, "error");
        p.message = json_string_at(obj, "message");
        p.traceback = json_string_at(obj, "traceback");
        return p;
    }

    p.status = obj.at("status").as_string();
    p.exam_id = json_string_at(obj, "exam_id");

    if (obj.contains("results") && obj.at("results").is_array())
        for (const auto& entry : obj.at("results").as_array())
            p.results.push_back(model_result_entry_from_json(entry));

    if (obj.contains("reports") && obj.at("reports").is_array())
        for (const auto& entry : obj.at("reports").as_array())
            p.reports.push_back(model_report_entry_from_json(entry));

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
