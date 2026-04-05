.PHONY: firmware firmware-tune firmware-clean firmware-flash firmware-tune-flash clean public

firmware:
	./firmware/scripts/build.sh

firmware-tune:
	./firmware/scripts/build.sh --target tune

firmware-clean:
	./firmware/scripts/build.sh --clean

firmware-flash:
	./firmware/scripts/flash.sh

firmware-tune-flash:
	./firmware/scripts/flash.sh ./firmware/build/nfc_tune.elf

clean: firmware-clean

# Build sanitized public tree into out/ (see .public-allowlist)
public:
	./scripts/build_public_tree.sh
