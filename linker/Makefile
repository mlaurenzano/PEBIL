TEXTADDR_REPL   = s/PROVIDE (__executable_start = 0x[0-9,a-f]*); . = 0x[0-9,a-f]* + SIZEOF_HEADERS;/PROVIDE (__executable_start = 0x08048000); . = 0x08048000 + SIZEOF_HEADERS;/
LDSCRIPT_BASE   = x86inst_ld_script__
LDSCRIPT        = $(LDSCRIPT_BASE)`hostname`
LINKER          = /usr/bin/ld.x

default:
	$(LINKER) --verbose | awk -f extractldscript.awk | sed '$(TEXTADDR_REPL)' > $(LDSCRIPT)

clean:
	rm $(LDSCRIPT_BASE)*