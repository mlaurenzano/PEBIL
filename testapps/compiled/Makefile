64BITPROGS_SER = cTest.gnu cppTest.gnu htcoef.gnu dynTest.gnu cTest.intel cppTest.intel htcoef.intel dynTest.intel cTest.pgi cppTest.pgi htcoef.pgi dynTest.pgi
64BITPROGS_PAR = xmesh_specfem3D.pgi hycom.standard wrf.standard
32BITPROGS_SER = cTest.gnu cppTest.gnu htcoef.gnu dynTest.gnu
32BITPROGS_PAR =
X86INSTREGEXP  = "(Instrumentation Successfull)"
DIFFREGEXP     = "are identical"
TESTAPPREGEXP  = "(Test Application Successfull)"
INSTCODEREGEXP = "(Instrumentation Code Successfull)"
SRC            = ../../src
BIN            = ../../bin
TESTBINS       = .
32BITBIN       = $(TESTBINS)/32bit/
64BITBIN       = $(TESTBINS)/64bit/
#LEAKCHECK      = valgrind --leak-check=yes
INSTRUMENTOR   = $(LEAKCHECK) $(BIN)/x86inst
INSTFLAGS      = --lib ../../lib --dtl
IDEEXT         = ideinst
FNCEXT         = fncinst
JBBEXT         = jbbinst
STATICEXT      = static
64BITPROGS     = $(64BITPROGS_SER) $(64BITPROGS_PAR)
32BITPROGS     = $(32BITPROGS_SER) $(32BITPROGS_PAR)


all: ide fnc
ide: ide32 ide64
fnc: fnc32 fnc64
jbb: jbb32 jbb64
32:  ide32 fnc32 jbb32
64:  ide64 fnc64 jbb64

include rules.$(BITS)

clean:
	rm -f $(32BITBIN)/*.$(IDEEXT)
	rm -f $(32BITBIN)/*.$(FNCEXT)
	rm -f $(32BITBIN)/*.$(JBBEXT)
	rm -f $(32BITBIN)/*.$(STATICEXT)
	rm -f $(64BITBIN)/*.$(IDEEXT)
	rm -f $(64BITBIN)/*.$(FNCEXT)
	rm -f $(64BITBIN)/*.$(JBBEXT)
	rm -f $(64BITBIN)/*.$(STATICEXT)


ide32:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ ide $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
		diff -s $(32BITBIN)/"$$i" $(32BITBIN)/"$$i".$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(32BITBIN)/*.$(IDEEXT)

ide64:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ ide $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
		diff -s $(64BITBIN)/"$$i" $(64BITBIN)/"$$i".$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(64BITBIN)/*.$(IDEEXT)

fnc32inst:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ fnc $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done

fnc32run:
	for i in ${32BITPROGS_SER}; do \
		$(32BITBIN)/"$$i".$(FNCEXT) | egrep $(TESTAPPREGEXP); \
	done	

fnc32ide:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i".$(FNCEXT) --typ ide $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done
	rm -f $(32BITBIN)/*.$(IDEEXT)

fnc32rm:
	rm -f $(32BITBIN)/*.$(FNCEXT)

fnc64inst:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ fnc $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done

fnc64run:
	for i in ${64BITPROGS_SER}; do \
		$(64BITBIN)/"$$i".$(FNCEXT) | egrep $(TESTAPPREGEXP); \
	done

fnc64ide:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i".$(FNCEXT) --typ ide $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done
	rm -f $(64BITBIN)/*.$(IDEEXT)

fnc64rm:
	rm -f $(64BITBIN)/*.$(FNCEXT)


jbb32inst:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ jbb $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done

jbb32run:
	for i in ${32BITPROGS_SER}; do \
		$(32BITBIN)/"$$i".$(JBBEXT) | egrep $(TESTAPPREGEXP); \
	done	

jbb32ide:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i".$(JBBEXT) --typ ide $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done
	rm -f $(32BITBIN)/*.$(IDEEXT)

jbb32rm:
	rm -f $(32BITBIN)/*.$(JBBEXT)

jbb64inst:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ jbb $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done

jbb64run:
	for i in ${64BITPROGS_SER}; do \
		$(64BITBIN)/"$$i".$(JBBEXT) | egrep $(TESTAPPREGEXP); \
	done

jbb64ide:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i".$(JBBEXT) --typ ide $(INSTFLAGS) | egrep $(X86INSTREGEXP); \
	done
	rm -f $(64BITBIN)/*.$(IDEEXT)

jbb64rm:
	rm -f $(64BITBIN)/*.$(JBBEXT)

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
