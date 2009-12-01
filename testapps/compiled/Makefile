64BITPROGS_SER = cTest.gnu cppTest.gnu htcoef.gnu dynTest.gnu cTest.intel cppTest.intel htcoef.intel dynTest.intel cTest.pgi cppTest.pgi htcoef.pgi dynTest.pgi
64BITPROGS_PAR = xmesh_specfem3D.pgi hycom.standard wrf.standard
32BITPROGS_SER = cTest.gnu cppTest.gnu htcoef.gnu dynTest.gnu
32BITPROGS_PAR =
PEBILREGEXP  = "(Instrumentation Successfull)"
DIFFREGEXP     = "are identical"
TESTAPPREGEXP  = "(Test Application Successfull)"
INSTCODEREGEXP = "(Instrumentation Code Successfull)"
SRC            = ../../src
BIN            = ../../bin
TESTBINS       = .
32BITBIN       = $(TESTBINS)/32bit/
64BITBIN       = $(TESTBINS)/64bit/
#LEAKCHECK      = valgrind --leak-check=yes
INST_OPTS      = --fbl ../../scripts/exclusion/system.func
INSTRUMENTOR   = $(LEAKCHECK) $(BIN)/pebil $(INST_OPTS)
INSTFLAGS      = --lib ../../lib --dtl
IDEEXT         = ideinst
FNCEXT         = fncinst
JBBEXT         = jbbinst
SIMEXT         = siminst
SIM_OPTS       = --inp foobarnada
STATICEXT      = static
64BITPROGS     = $(64BITPROGS_SER) #$(64BITPROGS_PAR)
32BITPROGS     = $(32BITPROGS_SER) $(32BITPROGS_PAR)


all: ide fnc
ide: ide32 ide64
fnc: fnc32 fnc64
jbb: jbb32 jbb64
sim: sim32 sim64
32:  ide32 fnc32 jbb32 sim32
64:  ide64 fnc64 jbb64 sim64

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
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ ide $(INSTFLAGS) | egrep $(PEBILREGEXP); \
		diff -s $(32BITBIN)/"$$i" $(32BITBIN)/"$$i".$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(32BITBIN)/*.$(IDEEXT)

ide64:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ ide $(INSTFLAGS) | egrep $(PEBILREGEXP); \
		diff -s $(64BITBIN)/"$$i" $(64BITBIN)/"$$i".$(IDEEXT) | egrep $(DIFFREGEXP); \
	done
	rm -f $(64BITBIN)/*.$(IDEEXT)

fnc32inst:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ fnc $(INSTFLAGS) | egrep $(PEBILREGEXP); \
	done

fnc32run:
	for i in ${32BITPROGS_SER}; do \
		$(32BITBIN)/"$$i".$(FNCEXT) | egrep $(TESTAPPREGEXP); \
	done	

fnc32rm:
	rm -f $(32BITBIN)/*.$(FNCEXT)

fnc64inst:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ fnc $(INSTFLAGS) | egrep $(PEBILREGEXP); \
	done

fnc64run:
	for i in ${64BITPROGS_SER}; do \
		$(64BITBIN)/"$$i".$(FNCEXT) | egrep $(TESTAPPREGEXP); \
	done

fnc64rm:
	rm -f $(64BITBIN)/*.$(FNCEXT)


jbb32inst:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ jbb $(INSTFLAGS) | egrep $(PEBILREGEXP); \
	done

jbb32run:
	for i in ${32BITPROGS_SER}; do \
		$(32BITBIN)/"$$i".$(JBBEXT) | egrep $(TESTAPPREGEXP); \
	done	

jbb32rm:
	rm -f $(32BITBIN)/*.$(JBBEXT)

jbb64inst:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) --app $(64BITBIN)/"$$i" --typ jbb $(INSTFLAGS) | egrep $(PEBILREGEXP); \
	done

jbb64run:
	for i in ${64BITPROGS_SER}; do \
		$(64BITBIN)/"$$i".$(JBBEXT) | egrep $(TESTAPPREGEXP); \
	done

jbb64rm:
	rm -f $(64BITBIN)/*.$(JBBEXT)

sim32inst:
	for i in ${32BITPROGS}; do \
		$(INSTRUMENTOR) --app $(32BITBIN)/"$$i" --typ sim $(INSTFLAGS) | egrep $(PEBILREGEXP); \
	done

sim32run:
	for i in ${32BITPROGS_SER}; do \
		$(32BITBIN)/"$$i".$(SIMEXT) | egrep $(TESTAPPREGEXP); \
	done	

sim32rm:
	rm -f $(32BITBIN)/*.$(SIMEXT)

sim64inst:
	for i in ${64BITPROGS}; do \
		$(INSTRUMENTOR) $(SIM_OPTS) --app $(64BITBIN)/"$$i" --typ sim $(INSTFLAGS) | egrep $(PEBILREGEXP); \
	done

sim64run:
	for i in ${64BITPROGS_SER}; do \
		$(64BITBIN)/"$$i".$(SIMEXT) | egrep $(TESTAPPREGEXP); \
	done

sim64rm:
	rm -f $(64BITBIN)/*.$(SIMEXT)

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
