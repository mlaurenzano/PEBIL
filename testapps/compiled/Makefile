X86INSTREGEXP  = "(Instrumentation Successfull)"
DIFFREGEXP     = "are identical"
TESTAPPREGEXP  = "(Test Application Successfull)"
SRC            = ../../src
TESTBINS       = .
32BITBIN       = $(TESTBINS)/32bit/
64BITBIN       = $(TESTBINS)/64bit/
EXECUTABLE     = $(SRC)/x86inst
IDEEXT         = ideinst
DATEXT         = datinst

all: ide dat
ide: ide32bit ide64bit
ide32bit:
	$(EXECUTABLE) --app $(32BITBIN)/cTest.gnu             --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(32BITBIN)/cTest.gnu $(32BITBIN)/cTest.gnu.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(32BITBIN)/cppTest.gnu           --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(32BITBIN)/cppTest.gnu $(32BITBIN)/cppTest.gnu.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(32BITBIN)/htcoef.gnu            --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(32BITBIN)/htcoef.gnu $(32BITBIN)/htcoef.gnu.$(IDEEXT) | egrep $(DIFFREGEXP)
	rm -f $(32BITBIN)/*.$(IDEEXT)

ide64bit:
	$(EXECUTABLE) --app $(64BITBIN)/cTest.gnu             --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cTest.gnu $(64BITBIN)/cTest.gnu.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cTest.sun             --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cTest.sun $(64BITBIN)/cTest.sun.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cTest.intel           --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cTest.intel $(64BITBIN)/cTest.intel.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cTest.pgi             --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cTest.pgi $(64BITBIN)/cTest.pgi.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.gnu           --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cppTest.gnu $(64BITBIN)/cppTest.gnu.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.sun           --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cppTest.sun $(64BITBIN)/cppTest.sun.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.intel         --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cppTest.intel $(64BITBIN)/cppTest.intel.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.pgi           --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/cppTest.pgi $(64BITBIN)/cppTest.pgi.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.gnu            --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/htcoef.gnu $(64BITBIN)/htcoef.gnu.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.sun            --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/htcoef.sun $(64BITBIN)/htcoef.sun.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.intel          --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/htcoef.intel $(64BITBIN)/htcoef.intel.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.pgi            --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/htcoef.pgi $(64BITBIN)/htcoef.pgi.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/xmesh_specfem3D.pgi   --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/xmesh_specfem3D.pgi $(64BITBIN)/xmesh_specfem3D.pgi.$(IDEEXT) | egrep $(DIFFREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/s3d_f90.x.H2          --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/s3d_f90.x.H2 $(64BITBIN)/s3d_f90.x.H2.$(IDEEXT) | egrep $(DIFFREGEXP)
	rm -f $(64BITBIN)/*.$(IDEEXT)

dat: dat32bit dat64bit
dat32bit:
	$(EXECUTABLE) --app $(32BITBIN)/cTest.gnu             --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(32BITBIN)/cTest.gnu.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(32BITBIN)/cppTest.gnu             --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(32BITBIN)/cppTest.gnu.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(32BITBIN)/htcoef.gnu             --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(32BITBIN)/htcoef.gnu.$(DATEXT) | egrep $(TESTAPPREGEXP)
	rm -f $(32BITBIN)/*.$(DATEXT)

dat64bit:
	$(EXECUTABLE) --app $(64BITBIN)/cTest.gnu             --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cTest.gnu.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cTest.sun             --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cTest.sun.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cTest.intel           --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cTest.intel.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cTest.pgi             --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cTest.pgi.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.gnu           --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cppTest.gnu.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.sun           --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cppTest.sun.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.intel         --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cppTest.intel.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/cppTest.pgi           --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/cppTest.pgi.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.gnu            --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/htcoef.gnu.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.sun            --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/htcoef.sun.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.intel          --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/htcoef.intel.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/htcoef.pgi            --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(64BITBIN)/htcoef.pgi.$(DATEXT) | egrep $(TESTAPPREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/xmesh_specfem3D.pgi   --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	$(EXECUTABLE) --app $(64BITBIN)/s3d_f90.x.H2   --typ dat --lib ../.. | egrep $(X86INSTREGEXP)
	rm -f $(64BITBIN)/*.$(DATEXT)
