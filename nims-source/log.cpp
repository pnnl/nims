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
#include <pthread.h>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_logger.hpp>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/support/date_time.hpp>

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;

/*

We use a singleton here in order to explicitly manage the lifetime
of the object. Using boost::log's global loggers is not safe during
a handler registered with atexit or possibly other times (it causes
a crash).

Note that this never gets deleted by design, as we want it to stick
around for the entire life of the program; the memory and resources
are only reclaimed when it terminates.

*/

static src::severity_logger_mt< logging::trivial::severity_level > *_shared_logger = NULL;

static void setup_shared_logger()
{
    assert(NULL == _shared_logger);
    _shared_logger = new src::severity_logger_mt< logging::trivial::severity_level >();
}

src::severity_logger_mt< logging::trivial::severity_level > * shared_logger()
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    (void) pthread_once(&once, setup_shared_logger);
    return _shared_logger;
}

void nims_perror(const char *context)
{
    // stash this away immediately in case a call changes it
    int err = errno;
    char buf[1024]{'\0'};
    // we end up with the GNU version instead of POSIX; don't use buf directly
    char *error_msg = strerror_r(err, buf, sizeof(buf));
    BOOST_LOG_SEV(*shared_logger(), logging::trivial::severity_level::error) << context << ": " << error_msg;
}

void setup_logging(std::string const & task_name, std::string const & level)
{    
    logging::trivial::severity_level slv;
    if (level == "trace")
        slv = logging::trivial::trace;
    else if (level == "debug")
        slv = logging::trivial::debug;
    else if (level == "info")
        slv = logging::trivial::info;
    else if (level == "warning")
        slv = logging::trivial::warning;
    else if (level == "error")
        slv = logging::trivial::error;
    else if (level == "fatal")
        slv = logging::trivial::fatal;
    
    logging::core::get()->set_filter
    (
        logging::trivial::severity >= slv
    );
    
    // adds date, pid
    logging::add_common_attributes();
    
    // add process name
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
    static pid_t pid = getpid();
    logging::add_console_log(
        std::clog,
        logging::keywords::format = (
            expr::stream << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S") << 
                " [" << logging::trivial::severity << "] " << 
                    expr::attr<std::string>("Task") << 
                        " (" << pid << ") " << 
                            expr::smessage
        )
    );
        
}

