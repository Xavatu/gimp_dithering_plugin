GIMPTOOL=gimptool-2.0
CFLAGS = -Wall -g -O3 `gimptool --cflags`
VERSION=0.1.3
DIR="dithering$(VERSION)"

FILES= \
	dithering.c \
	Makefile \
	Makefile.win \
	README \
	dithering.dev

all: dithering

dithering: dithering.c
	$(GIMPTOOL) --build dithering.c

install: dithering
	$(GIMPTOOL) --install-bin dithering

uninstall:
	$(GIMPTOOL) --uninstall dithering

dist: $(FILES)
	mkdir $(DIR)
	cp $(FILES) $(DIR)
	tar cvzf "$(DIR).tar.gz" $(DIR)
	rm -Rf $(DIR)

clean:
	rm -f dithering
