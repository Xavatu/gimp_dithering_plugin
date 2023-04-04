GIMPTOOL=gimptool-2.0
CFLAGS = -Wall -g -O3 `gimptool --cflags`
VERSION=0.1.3
DIR="gimp-plugin-dithering-$(VERSION)"

FILES= \
	src/dithering.c \
	src/colored_dithering.c \
	Makefile \
	LICENSE \
	README.md

PLUGS = dithering colored_dithering

all: $(PLUGS)

dithering: src/dithering.c
	$(GIMPTOOL) --build $<

colored_dithering: src/colored_dithering.c
	$(GIMPTOOL) --build $<

install: $(PLUGS)
	$(GIMPTOOL) --install-bin $(PLUGS)

uninstall:
	$(GIMPTOOL) --uninstall $(PLUGS)

dist: $(FILES)
	mkdir $(DIR)
	cp $(FILES) $(DIR)
	tar cvzf "$(DIR).tar.gz" $(DIR)
	rm -Rf $(DIR)

clean:
	rm -f $(PLUGS)
