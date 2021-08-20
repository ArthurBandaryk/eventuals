#pragma once

inline bool StoutGrpcLog(int level) {
  // TODO(benh): Initialize logging if it hasn't already been done so?
  static const char* variable = std::getenv("STOUT_GRPC_LOG");
  static int value = variable != nullptr ? atoi(variable) : 0;
  return value >= level;
}

#define STOUT_GRPC_LOG(level) LOG_IF(INFO, StoutGrpcLog(level))
