#pragma once
#include "Types.hpp"
#include <httplib.h>
#include <boost/json.hpp>
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
		cli_.set_connection_timeout( 5, 0 ); // 5 seconds
		cli_.set_read_timeout( 60, 0 );      // 60 seconds for report generation
	}


	bool IsHealthy()
	{
		if ( auto res = cli_.Get( "/health" ) )
		{
			if ( res->status == 200 )
			{
				auto j = boost::json::parse( res->body );
				return j.as_object()[ "status" ] == "ok";
			}
		}
		return false;
	}


	AnalysisResponse Analyze( const AnalysisRequest & request )
	{
		auto body = boost::json::serialize( to_json( request ) );
		if ( auto res = cli_.Post( "/analyze", body, "application/json" ) )
		{
			if ( res->status == 200 || res->status == 400 || res->status == 500 )
			{
				return analysis_response_from_json( boost::json::parse( res->body ) );
			}
			else
			{
				throw std::runtime_error( "Unexpected HTTP status: " + std::to_string( res->status ) );
			}
		}
		else
		{
			throw std::runtime_error( "Failed to connect to daemon" );
		}
	}


	void Shutdown()
	{
		cli_.Post( "/shutdown" );
	}

private:
	httplib::Client cli_;
};

} // namespace now_analysis
