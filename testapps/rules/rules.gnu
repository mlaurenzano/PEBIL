include rules/rules.common
COMP            = gnu
X86INST_CC      = gcc
X86INST_CPP     = g++ -DCPP
X86INST_F77     = g77
LINK_FLAGS      = $(HASH_STYLE) -Wl,-T,../linker/x86inst_ld_script__`hostname`
