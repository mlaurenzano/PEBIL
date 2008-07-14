X86INSTREGEXP  = "(Instrumentation Successfull)"
DIFFREGEXP     = "are identical"
SRC            = ../../src
TESTBINS       = .
32BITBIN       = $(TESTBINS)/32bit/
64BITBIN       = $(TESTBINS)/64bit/
EXECUTABLE     = $(SRC)/x86inst
IDEEXT         = ideinst


all: ide
ide: ide32bit ide64bit
ide32bit:
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
	$(EXECUTABLE) --app $(64BITBIN)/xmesh_specfem3D.pgi     --typ ide --lib ../.. | egrep $(X86INSTREGEXP)
	diff -s $(64BITBIN)/xmesh_specfem3D.pgi $(64BITBIN)/xmesh_specfem3D.pgi.$(IDEEXT) | egrep $(DIFFREGEXP)
