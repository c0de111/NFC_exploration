.PHONY: firmware firmware-clean firmware-flash clean

firmware:
	./firmware/scripts/build.sh

firmware-clean:
	./firmware/scripts/build.sh --clean

firmware-flash:
	./firmware/scripts/flash.sh

clean: firmware-clean
