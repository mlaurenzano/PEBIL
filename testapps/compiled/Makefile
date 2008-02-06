OUTEXT         = static
IDEEXT         = ideinst
JBBEXT         = jbbinst
SIMEXT         = siminst
TRCEXT         = bbtinst
CNTEXT         = cntinst
DATEXT         = datinst
CSCEXT         = cscinst

REGEXP		= "(Instrumentation Successfull)"

EXECUTABLE  = pmacinst

ide: ide32bit ide64bit
ide32bit:
	rm -f *.log *.$(OUTEXT) *.$(IDEEXT)
	$(EXECUTABLE) --app ./cTest       --typ ide | egrep $(REGEXP)
	$(EXECUTABLE) --app ./cppTest     --typ ide | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.gnu --typ ide | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.xlc --typ ide | egrep $(REGEXP)
	$(EXECUTABLE) --app ./hycom       --typ ide | egrep $(REGEXP)
ide64bit:
	$(EXECUTABLE) --app ./gamess      --typ ide | egrep $(REGEXP)

bbt: bbt32bit bbt64bit
bbt32bit:
	rm -f *.log *.$(OUTEXT) *.$(TRCEXT)
	$(EXECUTABLE) --app ./cTest       --typ bbt | egrep $(REGEXP)
	$(EXECUTABLE) --app ./cppTest     --typ bbt | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.gnu --typ bbt | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.xlc --typ bbt | egrep $(REGEXP)
	$(EXECUTABLE) --app ./hycom       --typ bbt | egrep $(REGEXP)
bbt64bit:
	$(EXECUTABLE) --app ./gamess      --typ bbt | egrep $(REGEXP)

jbb: jbb32bit jbb64bit
jbb32bit:
	rm -f *.log *.$(OUTEXT) *.$(JBBEXT)
	$(EXECUTABLE) --app ./cTest       --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./cppTest     --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.gnu --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.xlc --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./hycom       --typ jbb | egrep $(REGEXP)
jbb64bit:
	$(EXECUTABLE) --app ./gamess      --typ jbb | egrep $(REGEXP)

sim: sim32bit sim64bit
sim32bit:
	rm -f *.log *.$(OUTEXT) *.$(SIMEXT)
	$(EXECUTABLE) --app ./cTest       --typ sim --inp cTest.bb       | egrep $(REGEXP)
	$(EXECUTABLE) --app ./cppTest     --typ sim --inp cppTest.bb     | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.gnu --typ sim --inp meminst.gnu.bb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./meminst.xlc --typ sim --inp meminst.xlc.bb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./hycom       --typ sim --inp hycom.bb       | egrep $(REGEXP)
sim64bit:
	$(EXECUTABLE) --app ./gamess      --typ sim --inp gamess.bb      | egrep $(REGEXP)

mixed: 
	( cd ./3264; rm -f *.log *.$(OUTEXT) *.$(JBBEXT))
	$(EXECUTABLE) --app ./3264/gcc.32bit --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./3264/gcc.64bit --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./3264/gcC.32bit --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./3264/gcC.64bit --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./3264/xlc.32bit --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./3264/xlc.64bit --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./3264/xlC.32bit --typ jbb | egrep $(REGEXP)
	$(EXECUTABLE) --app ./3264/xlC.64bit --typ jbb | egrep $(REGEXP)

clean:
	rm -f core *.log *.$(OUTEXT) *.$(SIMEXT) *.$(JBBEXT) *.$(TRCEXT) *.$(IDEEXT) *.$(CNTEXT) *.$(DATEXT) *.$(CSCEXT) *.msim *.msim.blocks
	(cd ./3264; rm -f core *.log *.$(OUTEXT) *.$(JBBEXT))
