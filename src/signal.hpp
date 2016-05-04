// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#ifdef _WIN32
#define _USE_MATH_DEFINES // define math constants
#endif

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>

typedef enum sl_type {
	DEVICE_M1000 = 0x10000,
	CHANNEL_SMU = 0x20000,
	MODE_HIGH_Z = 0x40000,
	MODE_SVMI,
	MODE_SIMV,
	SIGNAL = 0x80000,
} sl_type;

typedef struct sl_unit {
	int8_t m;
	int8_t kg;
	int8_t s;
	int8_t A;
	int8_t K;
	int8_t mol;
	int8_t cd;
} sl_unit;

const sl_unit unit_V = { 2,  1, -3, -1,  0,  0,  0};
const sl_unit unit_A = { 0,  0,  0,  1,  0,  0,  0};

typedef struct sl_signal_info {
	sl_type type;

	const char* label;

	/// Bitmask of modes for which this signal is enabled as input
	uint32_t inputModes;

	/// Bitmask of modes for which this signal is enabled as output
	uint32_t outputModes;

	sl_unit unit;
	double min;
	double max;
	double resolution;
} sl_signal_info;

enum Dest {
	DEST_NONE,
	DEST_BUFFER,
	DEST_CALLBACK,
};

enum Src {
	SRC_CONSTANT,
	SRC_SQUARE,
	SRC_SAWTOOTH,
	SRC_STAIRSTEP,
	SRC_SINE,
	SRC_TRIANGLE,
	SRC_BUFFER,
	SRC_CALLBACK,
};

enum Modes {
	DISABLED,
	SVMI,
	SIMV,
};

class Signal {
	public:
		/// Get the descriptor struct of the Signal.
		/// Pointed-to memory is valid for the lifetime of the Device.
		const sl_signal_info* info() const { return m_info; }
		const sl_signal_info* const m_info;

		Src m_src;
		float m_src_v1;
		float m_src_v2;
		double m_src_period;
		double m_src_duty;
		double m_src_phase;

		float* m_src_buf;
		size_t m_src_i;
		size_t m_src_buf_len;
		bool m_src_buf_repeat;

		std::function<float (uint64_t index)> m_src_callback;

		Dest m_dest;

		// valid if m_dest == DEST_BUF
		float* m_dest_buf;
		size_t m_dest_buf_len;

		// valid if m_dest == DEST_CALLBACK
		std::function<void(float val)> m_dest_callback;

		/// internal: Do not call the constructor directly; obtain a Signal from a Device
		Signal(const sl_signal_info* info): m_info(info), m_src(SRC_CONSTANT), m_src_v1(0), m_dest(DEST_NONE) {}

		void update_phase(double new_period, double new_phase);

		void source_constant(float val);
		void source_square(float v1, float v2, double period, double duty, double phase);
		void source_sawtooth(float v1, float v2, double period, double phase);
		void source_stairstep(float v1, float v2, double period, double phase);
		void source_sine(float center, float amplitude, double period, double phase);
		void source_triangle(float v1, float v2, double period, double phase);
		void source_buffer(float* buf, size_t len, bool repeat);
		void source_callback(std::function<float (uint64_t index)> callback);

		/// Get the last measured sample from this signal.
		float measure_instantaneous() { return m_latest_measurement; }

		/// Configure received samples to be stored into `buf`, up to `len` points.
		/// After `len` points, samples will be dropped.
		void measure_buffer(float* buf, size_t len);

		/// Configure received samples to be passed to the provided callback.
		void measure_callback(std::function<void(float value)> callback);

		/// internal: Called by Device
		inline void put_sample(float val) {
			m_latest_measurement = val;
			if (m_dest == DEST_BUFFER) {
				if (m_dest_buf_len) {
					*m_dest_buf++ = val;
					m_dest_buf_len -= 1;
				}
			} else if (m_dest == DEST_CALLBACK) {
				m_dest_callback(val);
			}
		}

		/// internal: Called by Device
		inline float get_sample() {
			switch (m_src) {
			case SRC_CONSTANT:
				return m_src_v1;

			case SRC_BUFFER:
				if (m_src_i >= m_src_buf_len) {
					if (m_src_buf_repeat) {
						m_src_i = 0;
					} else {
						return m_src_buf[m_src_buf_len-1];
					}
				}
				return m_src_buf[m_src_i++];


			case SRC_CALLBACK:
				return m_src_callback(m_src_i++);

			case SRC_SQUARE:
			case SRC_SAWTOOTH:
			case SRC_SINE:
			case SRC_STAIRSTEP:
			case SRC_TRIANGLE:

				auto pkpk = m_src_v2 - m_src_v1;
				auto phase = m_src_phase;
				auto norm_phase = phase / m_src_period;
				if (norm_phase < 0) norm_phase += 1;
				m_src_phase = fmod(m_src_phase + 1, m_src_period);

				switch (m_src) {
				case SRC_SQUARE:
					return (norm_phase < m_src_duty) ? m_src_v1 : m_src_v2;

				case SRC_SAWTOOTH:
					return m_src_v2 - norm_phase * pkpk;

				case SRC_STAIRSTEP:
					return m_src_v2 - floorf(norm_phase*10)/10 * pkpk;

				case SRC_SINE:
					return m_src_v1 + (1 + cos(norm_phase * 2 * M_PI)) * pkpk/2;

				case SRC_TRIANGLE:
					return m_src_v1 + fabs(1 - norm_phase*2) * pkpk;
				default:
					return 0;
				}
			}
			return 0;
		}

	protected:
		float m_latest_measurement;
};
