// Released under the terms of the BSD License
// (C) 2014-2015
//   Analog Devices, Inc.
//   Tim Harder <radhermit@gmail.com>
//   Ian Daniher <itdaniher@gmail.com>
//   Ezra Varady <ez@sloth.life>

#include <cmath>
#include <Python.h>
#include <vector>
#include <queue>
#include <cstdint>
#include <functional>
#include <string>
#include <condition_variable>

#include "libsmu.hpp"

using namespace std::placeholders;
using std::vector;
using std::string;
using std::queue;
using std::mutex;

static Session* session = NULL; // Global session variable
static const uint32_t SAMPLE_RATE = 100000; // M1K sampling rate
std::condition_variable samples_available;
static mutex signal_mtx; // control continuous signal queue access
static queue <float> signal0_0, signal0_1, signal1_0, signal1_1;

// dummy struct for continuous mode iteration emulation
typedef struct {
	PyObject_HEAD
} inputs;

extern "C" {

	static PyObject* initSession(PyObject* self, PyObject* args){
		int ret;

		if (session == NULL)
			session = new Session();
		ret = session->update_available_devices();
		if (ret != 0)
			Py_RETURN_FALSE;
		Py_RETURN_TRUE;
	}

	static PyObject* cleanupSession(PyObject* self, PyObject* args){
		session->end();
		return PyInt_FromLong(0l);
	}

	static PyObject* getDevInfo(PyObject* self, PyObject* args){
		PyObject* data = PyList_New(0);
		for (auto i: session->m_available_devices){
			session->add_device(&*i);
		}
		for (auto dev: session->m_devices) {
			auto dev_info = dev->info();
			PyObject* dev_data = PyDict_New();
			for (unsigned chan=0; chan < dev_info->channel_count; chan++) {
				auto chan_info = dev->channel_info(chan);
				dev->set_mode(chan, 1);
				PyObject* sigs = PyList_New(0);

				for (unsigned sig=0; sig < chan_info->signal_count; sig++) {
					auto info = dev->signal(chan, sig)->info()->label;
					PyObject* info_str = PyString_FromString(string(info).c_str());
					PyList_Append(sigs, info_str);
					Py_DECREF(info_str);
				}
				PyObject* chan_label = PyString_FromString(string(chan_info->label).c_str());
				PyDict_SetItem(dev_data, chan_label, sigs);
				Py_DECREF(chan_label);
				Py_DECREF(sigs);
			}
			PyObject* serial_str = PyString_FromString(dev->serial());
			PyObject* ret = PyTuple_Pack(2, serial_str, dev_data);
			Py_DECREF(serial_str);
			Py_DECREF(dev_data);
			PyList_Append(data, ret);
			Py_DECREF(ret);
		}
		return data;
	}

	static Device* get_device(const char *dev_serial) {
		if (strlen(dev_serial) != 31) {
			PyErr_SetString(PyExc_ValueError, "invalid device serial number");
			return NULL;
		}
		auto dev = session->get_device(dev_serial);
		if (dev == NULL) {
			PyErr_SetString(PyExc_ValueError, "device not found");
			return NULL;
		}
		return dev;
	}

	static PyObject* setMode(PyObject* self, PyObject* args){
		const char *dev_serial;
		int chan_num;
		int mode_num;
		if (!PyArg_ParseTuple(args, "sii", &dev_serial, &chan_num, &mode_num))
			return NULL;

		auto dev = get_device(dev_serial);
		if (dev == NULL)
			return NULL;
		dev->set_mode((unsigned) chan_num, (unsigned) mode_num);
		Py_RETURN_NONE;
	}

	static PyObject* getInputs(PyObject* self, PyObject* args){
		const char *dev_serial;
		int chan_num;
		int nsamples;
		if (!PyArg_ParseTuple(args, "sii", &dev_serial, &chan_num, &nsamples))
			return NULL;

		auto dev = get_device(dev_serial);
		if (dev == NULL)
			return NULL;
		auto sgnl_v = dev->signal(chan_num, 0);
		auto sgnl_i = dev->signal(chan_num, 1);
		vector<float> buf_v;
		vector<float> buf_i;
		buf_v.resize(nsamples);
		buf_i.resize(nsamples);
		sgnl_v->measure_buffer(buf_v.data(), nsamples);
		sgnl_i->measure_buffer(buf_i.data(), nsamples);
		session->configure(SAMPLE_RATE);
		session->run(nsamples);
		PyObject* samples = PyList_New(0);
		for (int i = 0; i < nsamples; i++) {
			PyObject* sample_tuple = Py_BuildValue("(f,f)", buf_v[i], buf_i[i]);
			PyList_Append(samples, sample_tuple);
			Py_DECREF(sample_tuple);
		}
		return samples;
	}

	static PyObject* getAllInputs(PyObject* self, PyObject* args) {
		const char *dev_serial;
		int nsamples; /* number of samples to acquire */
		unsigned num_channels; /* number of channels passed */
		vector< vector<float> > buf_v, buf_i; /* 2d vector of voltage/current samples per channel */

		if (!PyArg_ParseTuple(args, "si", &dev_serial, &nsamples))
			return NULL;

		auto dev = get_device(dev_serial);
		if (dev == NULL)
			return NULL;

		num_channels = dev->info()->channel_count;
		buf_v.resize(num_channels);
		buf_i.resize(num_channels);
		for (unsigned i = 0; i < num_channels; i++) {
			buf_v[i].resize(nsamples);
			buf_i[i].resize(nsamples);
			dev->signal(i, 0)->measure_buffer(buf_v[i].data(), nsamples);
			dev->signal(i, 1)->measure_buffer(buf_i[i].data(), nsamples);
		}

		session->configure(SAMPLE_RATE);
		session->run(nsamples);

		PyObject* all_samples = PyList_New(0);
		for (unsigned i = 0; i < num_channels; i++) {
			PyObject* samples = PyList_New(0);
			for (int j = 0; j < nsamples; j++) {
				PyObject* sample_tuple = Py_BuildValue("(f,f)", buf_v[i][j], buf_i[i][j]);
				PyList_Append(samples, sample_tuple);
				Py_DECREF(sample_tuple);
			}
			PyList_Append(all_samples, samples);
			Py_DECREF(samples);
		}
		return all_samples;
	}

	static PyObject* setOutputWave(PyObject* self, PyObject* args){
		const char *dev_serial;
		int chan_num;
		int mode;
		int wave;
		double duty;
		double period;
		double phase;
		float val1;
		float val2;

		if (!PyArg_ParseTuple(args, "siiiffddd", &dev_serial, &chan_num, &mode,
							  &wave, &val1, &val2, &period, &phase, &duty))
			return NULL;

		auto dev = get_device(dev_serial);
		if (dev == NULL)
			return NULL;
		auto sgnl =  dev->signal(chan_num, 0);
		if (mode == SIMV)
			sgnl = dev->signal(chan_num, 1);
		if (wave == SRC_SQUARE)
			sgnl->source_square(val1, val2, period, duty, phase);
		if (wave == SRC_SAWTOOTH)
			sgnl->source_sawtooth(val1, val2, period, phase);
		if (wave == SRC_STAIRSTEP)
			sgnl->source_stairstep(val1, val2, period, phase);
		if (wave == SRC_TRIANGLE)
			sgnl->source_triangle(val1, val2, period, phase);
		if (wave == SRC_SINE)
			sgnl->source_sine(val1, val2, period, phase);
		Py_RETURN_NONE;
	}

	static PyObject* setOutputConstant(PyObject* self, PyObject* args){
		const char *dev_serial;
		int chan_num;
		int mode;
		float val;
		if (!PyArg_ParseTuple(args, "siif", &dev_serial, &chan_num, &mode, &val))
			return NULL;

		auto dev = get_device(dev_serial);
		if (dev == NULL)
			return NULL;
		if (mode == SVMI) {
			auto sgnl_v = dev->signal(chan_num, 0);
			sgnl_v->source_constant(val);
		}
		if (mode == SIMV) {
			auto sgnl_i = dev->signal(chan_num, 1);
			sgnl_i->source_constant(val);
		}
		Py_RETURN_NONE;
	}

	static PyObject* setOutputArbitrary(PyObject* self, PyObject* args){
		PyObject* buf;
		const char *dev_serial;
		int chan_num;
		int mode;
		int repeat;
		if (!PyArg_ParseTuple(args, "Osiii", &buf, &dev_serial, &chan_num, &mode, &repeat))
			return NULL;
		size_t buf_len = PyObject_Length(buf);
		float* dev_buf = (float*)(malloc(sizeof(float) * buf_len));
		for (size_t i = 0; i < buf_len; i++){
			PyObject* val = PySequence_GetItem(buf, i);
			if (!PyFloat_Check(val)){
				free(dev_buf);
				PyErr_SetString(PyExc_TypeError, "set_output_buffer() first arg must be a list of floats");
				return NULL;
			}
			char* tmp = (char*)malloc(100);
			double intermediary = PyFloat_AsDouble(val);
			sprintf(tmp, "%f", intermediary);
			dev_buf[i] = atof(tmp);
			free(tmp);
		}

		auto dev = get_device(dev_serial);
		if (dev == NULL) {
			free(dev_buf);
			return NULL;
		}
		auto sgnl = dev->signal(chan_num, mode);
		bool flag = false;
		if (repeat>0)
			flag = true;
		sgnl->source_buffer(dev_buf, buf_len, flag);
		Py_RETURN_NONE;
	}

	static PyObject* ctrlTransfer(PyObject* self, PyObject* args){
		const char *dev_serial;
		unsigned bmRequestType;
		unsigned bRequest;
		unsigned wValue;
		unsigned wIndex;
		unsigned char* data_use;
		PyObject* data;
		unsigned wLength;
		unsigned timeout;

		if (!PyArg_ParseTuple(args, "sIIIISII", &dev_serial, &bmRequestType, &bRequest, &wValue, &wIndex, &data, &wLength, &timeout))
			return NULL;
		data_use = (unsigned char*)PyString_AsString(data);
		if (*data_use == '0')
			data_use = nullptr;

		auto dev = get_device(dev_serial);
		if (dev == NULL)
			return NULL;
		dev->ctrl_transfer(bmRequestType, bRequest, wValue, wIndex, data_use, wLength, timeout);
		Py_RETURN_NONE;
	}

	static PyObject* inputs_iternext(inputs *self) {
		// don't read/pop values from the queues while adding to them
		std::unique_lock<mutex> lock(signal_mtx);

		// wait for complete samples to exist in the queue
		while (signal0_0.empty() || signal0_1.empty() || signal1_0.empty() || signal1_1.empty()) {
			samples_available.wait(lock);
		}

		PyObject* samples_tuple = Py_BuildValue("((f,f),(f,f))",
			signal0_0.front(), signal0_1.front(), signal1_0.front(), signal1_1.front());

		signal0_0.pop();
		signal0_1.pop();
		signal1_0.pop();
		signal1_1.pop();
		lock.unlock();

		return samples_tuple;
	}

	static void inputs_dealloc(inputs *self) {
		session->cancel();
		session->end();

		// flush buffer queues on iterator deallocation
		signal0_0 = queue<float>();
		signal0_1 = queue<float>();
		signal1_0 = queue<float>();
		signal1_1 = queue<float>();
	}

	static PyTypeObject inputs_type = {
		PyObject_HEAD_INIT(NULL)
		0,                         /* ob_size */
		"inputs._MyIter",  /* tp_name */
		sizeof(inputs),    /* tp_basicsize */
		0,                         /* tp_itemsize */
		(destructor)inputs_dealloc, /* tp_dealloc */
		0,                         /* tp_print */
		0,                         /* tp_getattr */
		0,                         /* tp_setattr */
		0,                         /* tp_compare */
		0,                         /* tp_repr */
		0,                         /* tp_as_number */
		0,                         /* tp_as_sequence */
		0,                         /* tp_as_mapping */
		0,                         /* tp_hash  */
		0,                         /* tp_call */
		0,                         /* tp_str */
		0,                         /* tp_getattro */
		0,                         /* tp_setattro */
		0,                         /* tp_as_buffer */
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
			/* tp_flags: Py_TPFLAGS_HAVE_ITER tells python to
			   use tp_iter and tp_iternext fields. */
		"Signal inputs iterator",  /* tp_doc */
		0,  /* tp_traverse */
		0,  /* tp_clear */
		0,  /* tp_richcompare */
		0,  /* tp_weaklistoffset */
		(getiterfunc)PyObject_SelfIter,  /* tp_iter */
		(iternextfunc)inputs_iternext,  /* tp_iternext: */
	};

	static PyObject * inputs_iter(PyObject *self, PyObject *args) {
		const char *dev_serial;
		inputs *p = NULL;

		if (!PyArg_ParseTuple(args, "s", &dev_serial))
			return NULL;

		auto dev = get_device(dev_serial);
		if (dev == NULL)
			return NULL;

		p = PyObject_New(inputs, &inputs_type);
		if (!p)
			return NULL;

		auto signal_callback = [](queue <float> *signal_q, float sample) {
			std::unique_lock<mutex> lock(signal_mtx);
			if ((*signal_q).size() < 1024)
				(*signal_q).push(sample);
			lock.unlock();
			if ((signal0_0.size() == 1) && (signal0_1.size() == 1) &&
					(signal1_0.size() == 1) && (signal1_1.size() == 1))
				samples_available.notify_one();
		};

		// setup signal callbacks
		dev->signal(0, 0)->measure_callback(std::bind(signal_callback, &signal0_0, _1));
		dev->signal(0, 1)->measure_callback(std::bind(signal_callback, &signal0_1, _1));
		dev->signal(1, 0)->measure_callback(std::bind(signal_callback, &signal1_0, _1));
		dev->signal(1, 1)->measure_callback(std::bind(signal_callback, &signal1_1, _1));

		session->configure(SAMPLE_RATE);
		// run in continuous mode
		session->start(0);

		return (PyObject *)p;
	}

	static PyMethodDef libpysmu_methods [] = {
		{ "setup", initSession, METH_VARARGS, "start session"  },
		{ "get_dev_info", getDevInfo, METH_VARARGS, "get device information"  },
		{ "cleanup", cleanupSession, METH_VARARGS, "end session"  },
		{ "set_mode", setMode, METH_VARARGS, "set channel mode"  },
		{ "get_inputs", getInputs, METH_VARARGS, "get measured voltage and current from a channel"  },
		{ "get_all_inputs", getAllInputs, METH_VARARGS, "get measured voltage and current from all channels"  },
		{ "iterate_inputs", inputs_iter, METH_VARARGS, "iterate over measured voltage and current from selected channels"  },
		{ "set_output_constant", setOutputConstant, METH_VARARGS, "set channel output - constant"  },
		{ "set_output_wave", setOutputWave, METH_VARARGS, "set channel output - wave"  },
		{ "set_output_buffer", setOutputArbitrary, METH_VARARGS, "set channel output - arbitrary wave"  },
		{ "ctrl_transfer", ctrlTransfer, METH_VARARGS, "initiate a control transfer"  },
		{ NULL, NULL, 0, NULL  }
	};

	DL_EXPORT(void) initlibpysmu(void)
	{
		Py_InitModule("libpysmu", libpysmu_methods);
	}
}
