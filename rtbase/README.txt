rtbase package purposes
#######################

The package proposes very basic C tools (light and portable):
	simplied poll mechanism
	iec104/apci links (xlap) with long message extension (250..16Kbytes)
	kernel linux list ported to user space
	kernel linux btree ported to user space
	hash tables
	simple inter thread communication based on message exchanges and timers
	X86/X64/ARM32/ARM64 supports

For each main features samples are given ~/rtbase/_directory_samples

Compilation and packaging
#########################

How to compile when you do not known the target system:
	./MAKE
	and select a system target in the proposed list.

How to compile when you known the target system:
	./MAKE system linux-x86_64
	you must be sure of the target system target, there is no control.

How to clean the rtbase directory:
	./MAKE clean
	this also remove the target system selected.

How to prepare a source tarball:
	./MAKE src
	this will produce $ROOTACT/archives/rtbase-a.b.c.tar.gz and the rtbase
	directory is cleaned.

How to prepare a runtime tarball (sources are removed, .a .so .h are kept):
	./MAKE runtime
	this will produce $ROOTACT/deliveries/rtbase-a.b.c-<target>.tar

