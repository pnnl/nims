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
#include <string>
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
#include <boost/log/sinks/text_file_backend.hpp>

#include <boost/filesystem.hpp>
#include "yaml-cpp/yaml.h"

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;
namespace fs = boost::filesystem;

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
    
    // adds date, pid
    logging::add_common_attributes();
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

void setup_logging(std::string const & task_name, std::string const & cfgpath, std::string const & level)
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
            expr::stream << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << 
                " [" << logging::trivial::severity << "] " << 
                    expr::attr<std::string>("Task") << 
                        " (" << pid << ")\t" << 
                            expr::smessage
        )
    );
    
    // need auto flush with file-based log
    // not sure yet if rotate works with append
        
    // log_dir should be something like /var/tmp which is world-writeable
    YAML::Node config = YAML::LoadFile(cfgpath);
    fs::path log_dir(config["LOG_DIR"].as<std::string>());
    
    // we need to munge the directory name, or only one user can run it per host
    // using "username-userid" is more readable than one by itself
    std::string uid_str = std::string("NIMS") + "-" + 
        getenv("USER") + "-" + boost::lexical_cast<std::string>(getuid());
    log_dir = log_dir / uid_str / "log";
    fs::path log_file = log_dir / fs::path(task_name + "_%N.log");
    
    logging::add_file_log(
        logging::keywords::file_name = log_file.string(),
        logging::keywords::rotation_size = 10 * 1024 * 1024,
        logging::keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(12, 0, 0),
        logging::keywords::auto_flush = true,
        keywords::open_mode = (std::ios::out | std::ios::app),
        logging::keywords::format = (
            expr::stream << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << 
                " [" << logging::trivial::severity << "] " << 
                    expr::attr<std::string>("Task") << 
                        " (" << pid << ")\t" << 
                            expr::smessage
        )
    );
    
    // mainly to separate log file blocks visually
    std::string uc_task_name;
    for (auto ch = task_name.begin(); ch != task_name.end(); ++ch)
        uc_task_name += std::toupper(*ch);
    NIMS_LOG_DEBUG << "**** WELCOME TO " << uc_task_name << " ****";
    NIMS_LOG_DEBUG << "log files at: " << log_dir;
    
}

