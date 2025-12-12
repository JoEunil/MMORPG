#pragma once
#include <cstdint>

inline constexpr const char* DB_HOST = "tcp://localhost";
inline constexpr const char* DB_USER = "root";
inline constexpr const char* DB_PASS = "1234";

inline constexpr const uint8_t DB_WORKER_POOL_SIZE = 2;

inline constexpr const uint16_t LOG_Q_SIZE = 8192;
inline constexpr const uint8_t LOG_THREAD_SIZE = 1;

inline constexpr const char* REDIS_AUTH_HOST = "127.0.0.1";
inline constexpr const uint16_t REDIS_AUTH_PORT = 6379;

