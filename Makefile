## change the following variables to fit your system:
DOXYGEN = doxygen

SUBDIRS = linker instcode testapps src tools

.PHONY: subdirs $(SUBDIRS)

all: subdirs

subdirs: $(SUBDIRS)

$(SUBDIRS): 
	$(MAKE) -C $@ 

verify:
	$(MAKE) -C testapps/compiled/

tools: src

doxygen:
	$(DOXYGEN) doxygen.cfg

clean:
	$(MAKE) -C src clean
	$(MAKE) -C testapps clean
	$(MAKE) -C instcode clean
	rm -rf bin/*

depend:
	$(MAKE) -C src depend
	$(MAKE) -C testapps depend
	$(MAKE) -C instcode depend




