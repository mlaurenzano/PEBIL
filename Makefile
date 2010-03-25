## change the following variables to fit your system:
DOXYGEN = doxygen

SUBDIRS = instcode testapps src tools
DISTDIR = PEBIL-0.1.`svnversion -n`

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

dist: clean
	mkdir $(DISTDIR)
	cp -r testapps src tools external bin docs include lib instcode scripts COPYING INSTALL Makefile $(DISTDIR)
	rm -rf $(DISTDIR)/.svn $(DISTDIR)/*/.svn $(DISTDIR)/*/*/.svn $(DISTDIR)/*/*/*/.svn $(DISTDIR)/*/*/*/*/.svn
	rm -rf $(DISTDIR)/testapps/compiled/32bit/* $(DISTDIR)/testapps/compiled/64bit/* $(DISTDIR)/testapps/bit
	tar czf $(DISTDIR).tgz $(DISTDIR)
	rm -rf $(DISTDIR)

