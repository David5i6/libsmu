// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include "device.hpp"
#include "session.hpp"
#include "signal.hpp"

#define LIBSMU_VERSION "0.8.9"

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define smu_debug(...) do { if (DEBUG_TEST) fprintf(stderr, __VA_ARGS__); } while(0);
