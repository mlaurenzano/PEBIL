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
FNCEXT         = fncinst
64BITPROGS     = $(64BITPROGS_SER) $(64BITPROGS_PAR)
32BITPROGS     = $(32BITPROGS_SER) $(32BITPROGS_PAR)


all: ide fnc
ide: ide32 ide64
fnc: fnc32 fnc64
include rules.$(BITS)

clean:
	rm -f $(32BITBIN)/*.$(IDEEXT)
	rm -f $(32BITBIN)/*.$(FNCEXT)
	rm -f $(64BITBIN)/*.$(IDEEXT)
	rm -f $(64BITBIN)/*.$(FNCEXT)


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

fnc32inst:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ fnc --lib ../.. | egrep $(X86INSTREGEXP); \
	done

fnc32run:
	for i in ${32BITPROGS_SER}; do \
		$(32BITBIN)/"$$i".$(FNCEXT) | egrep $(TESTAPPREGEXP); \
	done	

fnc32ide:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i".$(FNCEXT) --typ ide --lib ../.. | egrep $(X86INSTREGEXP); \
		diff -s $(32BITBIN)/"$$i".$(FNCEXT) $(32BITBIN)/"$$i".$(FNCEXT).$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(32BITBIN)/*.$(IDEEXT)

fnc32rm:
	rm -f $(32BITBIN)/*.$(FNCEXT)

fnc64inst:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ fnc --lib ../.. | egrep $(X86INSTREGEXP); \
	done

fnc64run:
	for i in ${64BITPROGS_SER}; do \
		$(64BITBIN)/"$$i".$(FNCEXT) | egrep $(TESTAPPREGEXP); \
	done

fnc64ide:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i".$(FNCEXT) --typ ide --lib ../.. | egrep $(X86INSTREGEXP); \
		diff -s $(64BITBIN)/"$$i".$(FNCEXT) $(64BITBIN)/"$$i".$(FNCEXT).$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(64BITBIN)/*.$(IDEEXT)

fnc64rm:
	rm -f $(64BITBIN)/*.$(FNCEXT)

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
