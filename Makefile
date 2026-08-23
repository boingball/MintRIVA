# MintVID top-level convenience targets.
#
# The portable host build lives in player/Makefile and the classic AmigaOS
# cross-build lives in player/Makefile.amiga.  Keep those specialised files
# as the source of truth; this wrapper provides the commands used day-to-day.

.PHONY: all clean submodules release

all:
	$(MAKE) -C player -f Makefile all

# Restore every submodule to the exact revision pinned by this MintVID
# checkout.  Do not use --remote here: release builds should be reproducible
# and must not silently pick up untested upstream commits.
submodules:
	git submodule sync --recursive
	git submodule update --init --recursive

# Clean both development/host artefacts and Amiga cross-build artefacts, then
# remove generated release/build trees so the next build starts from scratch.
clean:
	$(MAKE) -C player -f Makefile clean
	$(MAKE) -C player -f Makefile.amiga clean
	rm -rf player/build player/release

# The normal MintVID release: ensure pinned dependencies are present, then
# build the 68030/68040/68060 packages with AmiSSL and certificate checking.
release: submodules
	$(MAKE) -C player -f Makefile.amiga release SSL=1 SSLCERTS=1
