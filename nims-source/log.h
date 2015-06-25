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

__BEGIN_DECLS

/*
    Intended usage:
  
    #include <log.h>
  
    int main() {
  
      // initialize logging system and set up formats
      setup_logging(string(basename(argv[0])), "warning");
     
      // examples from http://www.boost.org/doc/libs/1_58_0/libs/log/doc/html/log/tutorial.html
      BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
      BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
      BOOST_LOG_TRIVIAL(info) << "An informational severity message";
  
      // with options passed to setup_logging, only these will print
      BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
      BOOST_LOG_TRIVIAL(error) << "An error severity message";
      BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";
  
      return 0;
      
    }
*/

void setup_logging(std::string const & task_name, std::string const & level="info");

__END_DECLS

#endif /* _LOG_H */