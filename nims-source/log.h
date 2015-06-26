/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  log.h
 *  
 *  Created by Adam Maxwell on 06/25/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef _LOG_H
#define _LOG_H 1

#include <string>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>

__BEGIN_DECLS

/*
    Intended usage:
  
    #include <log.h>
  
    int main() {
  
      // initialize logging system and set up formats
      setup_logging(string(basename(argv[0])), "warning");
     
      // examples from http://www.boost.org/doc/libs/1_58_0/libs/log/doc/html/log/tutorial.html
      NIMS_LOG_DEBUG << "A debug severity message";
  
      // with options passed to setup_logging, only these will print
      NIMS_LOG_WARNING << "A warning severity message";
      NIMS_LOG_ERROR << "An error severity message";
  
      return 0;
      
    }
*/

// this is part of the API, but use the macros instead
boost::log::sources::severity_logger_mt< boost::log::trivial::severity_level > * shared_logger();

// these three macros are the primary logging interface; each is a stream
#define NIMS_LOG_ERROR \
  BOOST_LOG_SEV(*shared_logger(), boost::log::trivial::severity_level::error)

#define NIMS_LOG_WARNING \
  BOOST_LOG_SEV(*shared_logger(), boost::log::trivial::severity_level::warning)

#define NIMS_LOG_DEBUG \
  BOOST_LOG_SEV(*shared_logger(), boost::log::trivial::severity_level::debug)
  
// call once from main() to register process name and set level
// supported level names: debug, warning, error
void setup_logging(std::string const & task_name, std::string const & level="debug");

// use this instead of perror to print context with errno
void nims_perror(const char *msg);

__END_DECLS

#endif /* _LOG_H */