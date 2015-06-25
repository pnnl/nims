/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  log.cpp
 *  
 *  Created by Adam Maxwell on 06/25/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include <log.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/support/date_time.hpp>
namespace logging = boost::log;

void set_log_level(int lvl, std::string const & task_name)
{
    namespace expr = boost::log::expressions;
    static pid_t pid = getpid();
    logging::core::get()->set_filter
    (
        logging::trivial::severity >= logging::trivial::info
    );
    logging::add_common_attributes();
    logging::core::get()->add_global_attribute("Task", logging::attributes::constant<std::string>(task_name));
    /*
    Notes
        a) boost::log adds ProcessID, but it only spits out values in hex.
        b) you apparently can't mix the tag format and this verbose format
        c) the tag format doesn't give "severity"
        d) the documentation for this is really confusing
    
    This is what I'm referring to as "tag" format:
        logging::add_console_log(std::cout, logging::keywords::format = "%Severity%: | %TimeStamp% | %Task% [%ProcessID%] | %Message%");
    
    Time format: http://www.boost.org/doc/libs/1_58_0/doc/html/date_time/date_time_io.html#format_strings
    */
    logging::add_console_log(
        std::clog,
        logging::keywords::format = (
            expr::stream << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%s") << 
                " [" << logging::trivial::severity << "] " << 
                    expr::attr<std::string>("Task") << 
                        " (" << pid << ") " << 
                            expr::smessage
        )
    );
        
}

