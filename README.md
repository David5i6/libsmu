### LibSMU

LibSMU contains abstractions for streaming data to and from USB-connected
analog interface devices, currently supporting the Nonolith CEE and Analog
Devices' ADALM1000. Building off of LibUSB for cross-platform operation, it
offers the sourcing of repeated waveforms, configuration of hardware, and
measuring of signals.

This project also includes 'pysmu,' an initial binding of LibSMU for Python2.7.

#### Building LibSMU

With libusb-1.0, and clang++, building LibSMU and the demo 'smu' executable is
as simple as typing `make`.

<<<<<<< HEAD
With python2.7 and associated development headers installed, building pysmu.so, importable as `import pysmu` is as simple as typing `make python`.

##### Notes on Mac OS X

Is recommended to install [homebrew](http://brew.sh)

Once installed, install lib-usb `brew install lib-usb`, pkg-config `brew install pkg-config` and python `brew install python`, restart OS and then build LibSMU. 
=======
With python2.7 and associated development headers installed, building pysmu.so,
importable as `import pysmu` is as simple as typing `make python`.

##### Notes on Mac OS X

First install [homebrew](http://brew.sh). Then use brew to install libusb, pkg-config, and python:

```
brew install libusb pkg-config python
```

Finally build libSMU:

```
git clone https://github.com/analogdevicesinc/libsmu.git
cd libsmu
make
```
>>>>>>> analogdevicesinc/master
