#pragma once
#include "Types.hpp"
#include <httplib.h>
#include <boost/json.hpp>
#include <optional>
#include <string>
#include <stdexcept>


namespace now_analysis
{

class Client
{
public:
	Client( const std::string & host = "127.0.0.1", int port = 8765 ) :
		cli_( host, port )
	{
		cli_.set_connection_timeout( 5, 0 );

		// HOW LONG TO WAIT FOR A REPLY. This was 60 s, set when /analyze was
		// the only call and took about a second.
		//
		// /model takes ~30 s for a three-test exam on the Windows development
		// machine (2026-10-21), and the model fit is most of it -- so it grows
		// with the number of tests and the length of each capture, neither of
		// which this client controls. Sixty seconds left a factor of two on a
		// call already measured at thirty.
		//
		// Three minutes BOUNDS "the daemon never came back" rather than
		// targeting a response time, the same reasoning as
		// DaemonManager::kShutdownGraceMs and NowAnalysis::kHealthTimeout. It
		// costs nothing when things work and costs a lost exam's analysis when
		// set too tight. /analyze is unaffected: a bound it never approaches.
		cli_.set_read_timeout( 180, 0 );
	}


	// The parsed /health reply, or nullopt if the daemon did not answer 200 with
	// a body that parses. Never throws: it is polled while the daemon starts.
	std::optional< HealthResponse > Health()
	{
		if ( auto res = cli_.Get( "/health" ) )
		{
			if ( res->status == 200 )
			{
				try
				{
					return health_response_from_json( boost::json::parse( res->body ) );
				}
				catch ( const std::exception & )
				{
				}
			}
		}
		return std::nullopt;
	}


	bool IsHealthy()
	{
		const auto health = Health();
		return health && health->status == "ok";
	}


	AnalysisResponse Analyze( const AnalysisRequest & request )
	{
		return analysis_response_from_json( PostJson( "/analyze", request ) );
	}


	// Fit the exam's models and draw its exam-level reports. ONE CALL PER EXAM:
	// see the note on ModelRequest for why this is not /analyze's business.
	//
	// Returns on a failed fit as well as a successful one -- status is "error"
	// and the per-entry `error` says which test and why. A test is reported,
	// never dropped. Throws only when the daemon could not be reached or
	// answered something outside the contract.
	ModelResponse Model( const ModelRequest & request )
	{
		return model_response_from_json( PostJson( "/model", request ) );
	}


	void Shutdown()
	{
		cli_.Post( "/shutdown" );
	}

private:
	// POST a request as JSON and hand back the parsed reply body.
	//
	// ONE COPY OF THE RULES BOTH ENDPOINTS SHARE: which HTTP statuses carry a
	// body worth parsing, and what to say when something else happens. 200,
	// 400 and 500 all carry a contract body -- the daemon puts a results
	// payload or an error envelope in each, and the status code does NOT tell
	// them apart, which is why discrimination happens on the body in
	// Types.hpp. Anything else is outside the contract and throws.
	template< typename Request >
	boost::json::value PostJson( const char * path, const Request & request )
	{
		const auto body = boost::json::serialize( to_json( request ) );
		auto res = cli_.Post( path, body, "application/json" );

		if ( !res )
		{
			// SAY WHICH FAILURE IT WAS. This used to be a flat "Failed to
			// connect to daemon", which is wrong for the case that actually
			// bites: a read timeout on a long /model looks identical to a
			// daemon that was never there. Distinguishing a slow daemon from
			// an absent one by its error message is the lesson of the
			// 2026-10-21 health-check bug, where exactly that ambiguity cost
			// a round trip.
			throw std::runtime_error( std::string{ "POST " } + path + " failed: "
			                          + httplib::to_string( res.error() ) );
		}

		if ( res->status != 200 && res->status != 400 && res->status != 500 )
		{
			// Carry the body: a 404 arrives as an error envelope naming the
			// path, and without it the caller is left with a bare status code.
			throw std::runtime_error( std::string{ "POST " } + path + " unexpected HTTP status: "
			                          + std::to_string( res->status ) + " body: " + res->body );
		}

		return boost::json::parse( res->body );
	}

	httplib::Client cli_;
};

} // namespace now_analysis
