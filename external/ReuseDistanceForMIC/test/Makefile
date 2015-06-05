TGT = test
TNAME=ReuseDistance
TLIB = lib$(TNAME).so

INCLUDE = -I..
LINK = -L.. -l$(TNAME)

ANSWER = answer.txt
CORRECT = correct.txt

CXX_MIC = icpc
MICFLAGS = -mmic
CXX = $(CXX_MIC)
CXXFLAGS = -g -O2 -std=c++0x -DHAVE_UNORDERED_MAP $(INCLUDE) $(MICFLAGS)

.PHONY: all clean depend check

all: $(TGT)

$(TGT): $(TGT).o
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

check: all $(ANSWER)
	! diff $(ANSWER) $(CORRECT) || echo "****** Tests successfully passed"

$(ANSWER):
	LD_LIBRARY_PATH=../:${LD_LIBRARY_PATH} ./$(TGT) > $@

clean:
	rm -rf $(TGT).o $(TGT) *.ii *.s $(ANSWER)

depend:
	g++ -E -MM $(INCLUDE) $(TGT).cpp > DEPENDS

include DEPENDS
