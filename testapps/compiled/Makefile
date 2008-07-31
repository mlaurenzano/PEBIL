64BITPROGS_SER = cTest.gnu cppTest.gnu htcoef.gnu cTest.intel cppTest.intel htcoef.intel cTest.sun cppTest.sun htcoef.sun cTest.pgi cppTest.pgi htcoef.pgi
64BITPROGS_PAR = #xmesh_specfem3D.pgi hycom.standard wrf.standard
32BITPROGS_SER = cTest.gnu cppTest.gnu htcoef.gnu
32BITPROGS_PAR =
X86INSTREGEXP  = "(Instrumentation Successfull)"
DIFFREGEXP     = "are identical"
TESTAPPREGEXP  = "(Test Application Successfull)"
SRC            = ../../src
TESTBINS       = .
32BITBIN       = $(TESTBINS)/32bit/
64BITBIN       = $(TESTBINS)/64bit/
#LEAKCHECK      = valgrind --leak-check=yes
INSTRUMENTOR   = $(LEAKCHECK) $(SRC)/x86inst
IDEEXT         = ideinst
DATEXT         = datinst
64BITPROGS     = $(64BITPROGS_SER) $(64BITPROGS_PAR)
32BITPROGS     = $(32BITPROGS_SER) $(32BITPROGS_PAR)


all: ide dat
ide: ide32 ide64
dat: dat32 dat64
include rules.$(BITS)


ide32:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ ide --lib ../.. | egrep $(X86INSTREGEXP); \
		diff -s $(32BITBIN)/"$$i" $(32BITBIN)/"$$i".$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(32BITBIN)/*.$(IDEEXT)

ide64:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ ide --lib ../.. | egrep $(X86INSTREGEXP); \
		diff -s $(64BITBIN)/"$$i" $(64BITBIN)/"$$i".$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(64BITBIN)/*.$(IDEEXT)

dat32inst:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ dat --lib ../.. | egrep $(X86INSTREGEXP); \
	done

dat32run:
	for i in ${32BITPROGS_SER}; do \
		chmod a+x $(32BITBIN)/"$$i".$(DATEXT); \
		$(32BITBIN)/"$$i".$(DATEXT) | egrep $(TESTAPPREGEXP); \
	done	

dat32ide:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i".$(DATEXT) --typ ide --lib ../.. | egrep $(X86INSTREGEXP); \
	done

dat32rm:
	rm -f $(32BITBIN)/*.$(DATEXT)

dat64inst:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ dat --lib ../.. | egrep $(X86INSTREGEXP); \
	done

dat64run:
	for i in ${64BITPROGS_SER}; do \
		chmod a+x $(64BITBIN)/"$$i".$(DATEXT); \
		$(64BITBIN)/"$$i".$(DATEXT) | egrep $(TESTAPPREGEXP); \
	done

dat64ide:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i".$(DATEXT) --typ ide --lib ../.. | egrep $(X86INSTREGEXP); \
	done

dat64rm:
	rm -f $(64BITBIN)/*.$(DATEXT)

nobitswarn:
	echo ""
	echo ""
	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	echo "---------------------------------------------------------------------------------------"
	echo "    WARNING: argument 'BITS=[32|64]' not given to makefile, not running any tests      "
	echo "---------------------------------------------------------------------------------------"
	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	echo ""
	echo ""
