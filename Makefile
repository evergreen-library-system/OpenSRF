all:
	@source install.conf && make -s -C src all

jserver:
	@source install.conf && make -s -C src jserver

install:
	@source install.conf && make -s -C src install

jserver-install:
	@source install.conf && make -s -C src jserver-install

clean:
	@make -s -C src clean
