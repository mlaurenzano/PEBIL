TEXTADDR_REPL   = s/PROVIDE (__executable_start = 0x[0-9,a-f]*); . = 0x[0-9,a-f]* + SIZEOF_HEADERS;/PROVIDE (__executable_start = 0x08048000); . = 0x08048000 + SIZEOF_HEADERS;/
LDSCRIPT        = x86inst_ld_script__`hostname`

default:
	ld --verbose | awk -f extractldscript.awk | sed '$(TEXTADDR_REPL)' > $(LDSCRIPT)

clean:
	rm $(LDSCRIPT)