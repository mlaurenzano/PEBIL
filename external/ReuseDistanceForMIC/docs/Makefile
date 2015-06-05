PDFTGT = ReuseDistance.pdf
HTMLTGT = ReuseDistance.html
MANTGT = ReuseDistance.man3

.PHONY: all doxygen

all: doxygen $(PDFTGT)

doxygen:
	doxygen

$(PDFTGT): doxygen
	cd latex; pdflatex refman.tex; cp refman.pdf $@;
	ln -s latex/$(PDFTGT) $(PDFTGT)

clean:
	rm -rf latex/ man/ html/ $(PDFTGT)
