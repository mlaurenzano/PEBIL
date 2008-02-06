## change the following variables to fit your system:
DOXYGEN = doxygen


SUBDIRS = src testapps

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

depend:
	$(MAKE) -C src depend
	$(MAKE) -C testapps depend
