obj-m += kernelware.o
kernelware-objs := kernelware_main.o kernelware_proc.o kw_games.o kw_state.o kw_timer.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(dir $(abspath $(lastword $(MAKEFILE_LIST)))) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(dir $(abspath $(lastword $(MAKEFILE_LIST)))) clean

reload:
	sudo rmmod kernelware || true
	make
	/usr/lib/modules/$(shell uname -r)/build/scripts/sign-file \
		sha512 signing_key.pem signing_cert.pem kernelware.ko
	sudo insmod kernelware.ko
	sudo dmesg | tail -5