L2 cache library
================

*****************
Install XCore SDK
*****************

See: https://github.com/xmos/xcore_sdk/blob/develop/documents/quick_start/installation.rst

*********************************
Building and running the firmware
*********************************

Make a directory for the build.

.. code-block:: console

    $ mkdir build
    $ cd build

To configure and build the firmware, run:

.. code-block:: console

    $ cmake ../
    $ make -j

To flash the SwMem section, run:

.. code-block:: console

    $ make flash

To run the firmware with hardware, run:

.. code-block:: console

    $ make run

***************************
Useful Build Configurations
***************************

The L2 cache has a debug mode which will track and printout some useful metrics, like hit rate.  Note that this
mode will make the timing metrics unreliable so those are not printed.  To configure and build with the L2 cache
in debug mode, run:

.. code-block:: console

    $ cmake ../ -DL2_CACHE_DEBUG=1
    $ make -j

The flash handler has a debug mode which will track and printout some useful metrics, like the number of flash reads and throughput.
To configure and build with the flash handler in debug mode, run:

.. code-block:: console

    $ cmake ../ -DFLASH_DEBUG=1
    $ make -j

To configure and build the firmware, placing all benchmark code and data in SRAM, run:

.. code-block:: console

    $ cmake ../ -DUSE_SWMEM=0
    $ make -j
