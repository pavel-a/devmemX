# devmem2

Program to read/write from/to any location in physical memory

This is enhanced variant of "devmem" or "devmem2" utility.
Source and documentation: http://git.io/vZ5iD

For Linux 2.6 - 3.x - 4.x


Origin: https://github.com/VCTLabs/devmem2
By lartware, 2004
http://www.lartmaker.nl/lartware/port/

Useful on ARM, especially on TI and other platforms with GPIO (eg,
beaglebone, raspberrypi).  Seems to be required by TI SGX/PowerVR
graphics and related tools.

This is just the upstream code with some cleanups and a simple
Makefile for Gentoo and OpenEmbedded.  To compile by hand, just
set CC and CFLAGS on the make command-line.
