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

void set_log_level(int lvl, std::string const & task_name);

__END_DECLS

#endif /* _LOG_H */