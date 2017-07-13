//
// Created by 60213 on 2017/7/12.
//

#ifndef SMALL_OC_FS_DBG_INFO_H
#define SMALL_OC_FS_DBG_INFO_H

#define OCSSD_DBG_ENABLED 1
#ifdef OCSSD_DBG_ENABLED

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

inline std::string methodName(const std::string& prettyFunction) {
  size_t begin = 0, end = 0;

  std::string prefix("small_oc_fs::");

  end = prettyFunction.find("(");
  if (end != std::string::npos) {
    begin = prettyFunction.rfind(prefix, end);
    begin += 9;
  }

  if ((begin < end) && (end < prettyFunction.length()))
    return prettyFunction.substr(begin, end-begin);

  return "";
}

#define __METHOD_NAME__ methodName(__PRETTY_FUNCTION__)
#define __FNAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define OCSSD_DBG_INFO_( x ) do {                                       \
  std::stringstream ss; ss                                              \
  << std::setfill('-') << std::setw(15) << std::left << __FNAME__       \
  << std::setfill('-') << std::setw(5)  << std::right << __LINE__       \
  << std::setfill(' ') << " "                                           \
  << std::setfill(' ') << std::setw(34) << std::left << __METHOD_NAME__ \
  << std::setfill(' ') << " "                                           \
  << std::setfill(' ') << x                                             \
  << std::endl;                                                         \
  fprintf(stdout, "%s", ss.str().c_str()); fflush(stdout);              \
} while (0);

#define OCSSD_DBG_INFO(obj, x) do {                                     \
  std::stringstream ss; ss                                              \
  << std::setfill('-') << std::setw(15) << std::left << __FNAME__       \
  << std::setfill('-') << std::setw(5)  << std::right << __LINE__       \
  << std::setfill(' ') << std::setw(20)  << std::right << obj->txt()    \
  << std::setfill(' ') << " "                                           \
  << std::setfill(' ') << std::setw(34) << std::left << __METHOD_NAME__ \
  << std::setfill(' ') << " "                                           \
  << std::setfill(' ') << x                                             \
  << std::endl;                                                         \
  fprintf(stdout, "%s", ss.str().c_str()); fflush(stdout);              \
} while (0);
#else
#define OCSSD_DBG_INFO(obj, x)
#endif

#endif //SMALL_OC_FS_DBG_INFO_H
