// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "signal.hpp"

void Signal::update_phase(double new_period, double new_phase) {
	m_src_phase = new_phase;
	m_src_period = new_period;
}

void Signal::source_constant(float val) {
	m_src = SRC_CONSTANT;
	m_src_v1 = val;
}

void Signal::source_square(float v1, float v2, double period, double duty, double phase) {
	m_src = SRC_SQUARE;
	update_phase(period, phase);
	m_src_v1 = v1;
	m_src_v2 = v2;
	m_src_duty = duty;
}

void Signal::source_sawtooth(float v1, float v2, double period, double phase) {
	m_src = SRC_SAWTOOTH;
	update_phase(period, phase);
	m_src_v1 = v1;
	m_src_v2 = v2;
}

void Signal::source_stairstep(float v1, float v2, double period, double phase) {
	m_src = SRC_STAIRSTEP;
	update_phase(period, phase);
	m_src_v1 = v1;
	m_src_v2 = v2;
}

void Signal::source_sine(float center, float amplitude, double period, double phase) {
	m_src = SRC_SINE;
	update_phase(period, phase);
	m_src_v1 = center;
	m_src_v2 = amplitude;
}

void Signal::source_triangle(float v1, float v2, double period, double phase) {
	m_src = SRC_TRIANGLE;
	update_phase(period, phase);
	m_src_v1 = v1;
	m_src_v2 = v2;
}

void Signal::source_buffer(float* buf, size_t len, bool repeat) {
	m_src = SRC_BUFFER;
	m_src_buf = buf;
	m_src_buf_len = len;
	m_src_buf_repeat = repeat;
	m_src_i = 0;
}

void Signal::source_callback(std::function<float (uint64_t index)> callback) {
	m_src = SRC_CALLBACK;
	m_src_callback = callback;
	m_src_i = 0;
}

/// Configure received samples to be stored into `buf`, up to `len` points.
/// After `len` points, samples will be dropped.
void Signal::measure_buffer(float* buf, size_t len) {
	m_dest = DEST_BUFFER;
	m_dest_buf = buf;
	m_dest_buf_len = len;
}

/// Configure received samples to be passed to the provided callback.
void Signal::measure_callback(std::function<void(float value)> callback) {
	m_dest = DEST_CALLBACK;
	m_dest_callback = callback;
}
