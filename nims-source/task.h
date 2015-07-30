/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  task.h
 *  
 *  Created by Adam Maxwell on 04/22/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef _TASK_H
#define _TASK_H 1

#include <string>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace nims {
  
class Task {
public:
  Task(const fs::path &absolute_path, const std::vector<std::string> &args);
  ~Task();
  pid_t get_pid() { return pid_; }
  void signal(int sig);
  bool launch();
  std::string name();
protected:
  pid_t                    pid_;
  fs::path                 absolute_path_;
  std::vector<std::string> arguments_;
};

} // namespace nims

#endif /* _TASK_H */
