#pragma once
#include <cstdio>
#define LOG_DBG(tag, format, ...) std::fprintf(stderr, tag ": " format "\n", ##__VA_ARGS__)
#define LOG_ERR LOG_DBG
