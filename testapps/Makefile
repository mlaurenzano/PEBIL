CC          = gcc
CXX         = g++
FC          = mpif90
MPICC       = mpicc

BINDIR      = ../src

OBJS        = foo.o bar.o dum.o main.o
OBJSCPP	    = classes.o
EXTRA_INC   = -I../instcode
EXTRA_LIBS  = -lm

TARGETS  = cTest cppTest fTest dynTest sgTest
IDETGT = $(subst Test,Test.ideinst,$(TARGETS))
JBBTGT = $(subst Test,Test.jbbinst,$(TARGETS))
SIMTGT = $(subst Test,Test.siminst,$(TARGETS))
LPITGT = $(subst Test,Test.lpiinst,$(TARGETS))
DISTGT = $(subst Test,Test.disasm,$(TARGETS))

all: $(TARGETS) 
	echo $(IDETGT)
%.o: %.c
	$(CC) -g -c -o $@ $< 

%.o: %.C
	$(CXX) -g -c -o $@ $< 

cTest: $(OBJS)
	$(CC) -g -o $@ $(OBJS)

cppTest: $(OBJSCPP)
	$(CXX) -g -o $@ $(OBJSCPP)

fTest:
	$(FC) -g -o $@ fTest.f

dynTest:
	$(CC) -g -o $@ dynTest.c

sgTest:
	$(CC) -g -o $@ sgTest.c $(EXTRA_INC) $(EXTRA_LIBS)

showme:
	which pebil
	ldd `which pebil`

check: showme $(IDETGT) $(JBBTGT) $(SIMTGT) $(LPITGT) $(DISTGT)
PEBIL_COMMAND = pebil --silent
PEBIL_COMMAND_I = $(PEBIL_COMMAND) --typ
PEBIL_COMMAND_T = $(PEBIL_COMMAND) --tool
FILTER_PEBIL_MSGS = grep -v "\-\["
FILTER_PEBIL_MSGS = grep -v "\[Metasim\-r"
LOOPS_FILE = all.loops
CREATE_LOOPS_FILE = echo "*:*:0" > $(LOOPS_FILE)
OUT = outp
DIFF = diff
NULL_FILE = /dev/null

%.ideinst: %
	$(PEBIL_COMMAND_I) ide --app $<
	$(DIFF) $< $@
	./$@ > $@.$(OUT)
	./$< > $<.$(OUT)
	$(DIFF) $<.$(OUT) $@.$(OUT)

%.jbbinst: %
	$(PEBIL_COMMAND_T) BasicBlockCounter --app $<
	./$@ | $(FILTER_PEBIL_MSGS) > $@.$(OUT)
	./$< > $<.$(OUT)
	$(DIFF) $<.$(OUT) $@.$(OUT)

%.siminst: %
	$(PEBIL_COMMAND_T) CacheSimulation --app $< --inp $(NULL_FILE)
	./$@ | $(FILTER_PEBIL_MSGS) > $@.$(OUT)
	./$< > $<.$(OUT)
	$(DIFF) $<.$(OUT) $@.$(OUT)

%.lpiinst: %
	$(CREATE_LOOPS_FILE)
	$(PEBIL_COMMAND_T) LoopIntercept --app $< --inp $(LOOPS_FILE) --lnc liblooptimer.so
	rm -f $(LOOPS_FILE)
	./$@ | $(FILTER_PEBIL_MSGS) > $@.$(OUT)
	./$< > $<.$(OUT)
	$(DIFF) $<.$(OUT) $@.$(OUT)

%.disasm: %
	check_disasm.py --file $<

clean: 
	rm -f *.o $(TARGETS) *.jbbinst *.loopcnt *.siminst *.ideinst *.static *.$(OUT)
