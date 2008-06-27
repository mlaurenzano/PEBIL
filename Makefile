## change the following variables to fit your system:
DOXYGEN = doxygen


SUBDIRS = src testapps instcode

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS): 
	$(MAKE) -C $@ 

tools: src

doxygen:
	$(DOXYGEN) doxygen.cfg

clean:
	$(MAKE) -C src clean
	$(MAKE) -C testapps clean
	$(MAKE) -C instcode clean

depend:
	$(MAKE) -C src depend
	$(MAKE) -C testapps depend
	$(MAKE) -C instcode depend
