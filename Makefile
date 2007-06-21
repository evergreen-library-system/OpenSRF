all:
	@echo -e "\n * Run 'make verbose' to see full make output\n"
	source install.conf && make -s -C src all

verbose:
	source install.conf && make -C src all

jserver:
	source install.conf && make -s -C src jserver

install:
	source install.conf && make -s -C src install

jserver-install:
	source install.conf && make -s -C src jserver-install

clean:
	make -s -C src clean

# test
