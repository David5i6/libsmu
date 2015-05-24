// Released under the terms of the BSD License
// (C) 2012-2015
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once
#include <mutex>
#include "libsmu.hpp"
#include "internal.hpp"

struct libusb_device_handle;
extern "C" void LIBUSB_CALL cee_in_completion(libusb_transfer *t);
extern "C" void LIBUSB_CALL cee_out_completion(libusb_transfer *t);

class CEE_Device: public Device {
public:
	virtual ~CEE_Device();
	virtual const sl_device_info* info() const;
	virtual const sl_channel_info* channel_info(unsigned channel) const;
	//virtual sl_mode_info* mode_info(unsigned mode);
	virtual Signal* signal(unsigned channel, unsigned signal);
	virtual void set_mode(unsigned channel, unsigned mode);
	virtual int get_default_rate();

protected:
	friend class Session;
	friend void LIBUSB_CALL cee_in_completion(libusb_transfer *t);
	friend void LIBUSB_CALL cee_out_completion(libusb_transfer *t);

	CEE_Device(Session* s, libusb_device* device);
	virtual int init();
	virtual int added();
	virtual int removed();
	virtual void configure(uint64_t sampleRate);
	virtual void start_run(sample_t nsamples);
	virtual void cancel();
	virtual void on();
	virtual void off();

	void in_completion(libusb_transfer *t);
	void out_completion(libusb_transfer *t);

	bool submit_out_transfer(libusb_transfer* t);
	bool submit_in_transfer(libusb_transfer* t);
	void handle_in_transfer(libusb_transfer* t);

	uint16_t encode_out(unsigned chan, uint32_t igain);

	std::string m_hw_version;
	std::string m_fw_version;
	std::string m_git_version;

	unsigned m_packets_per_transfer;
	Transfers m_in_transfers;
	Transfers m_out_transfers;

	struct EEPROM_cal{
		uint32_t magic;
		int8_t offset_a_v, offset_a_i, offset_b_v, offset_b_i;
		int16_t dac200_a, dac200_b, dac400_a, dac400_b;
		uint32_t current_gain_a, current_gain_b;
		uint8_t flags; // bit 0: USB powered
	}; //__attribute__((packed));

	void read_calibration();
	EEPROM_cal m_cal;

	void set_current_limit(unsigned mode);
	unsigned m_current_limit;
	int m_min_per;
	int m_xmega_per;

	uint64_t m_sample_count;
	Signal* m_signals[2][2];

	unsigned m_mode[2];
};
