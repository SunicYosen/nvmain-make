BASE_DIR    ?= $(abspath .)
vpitab_file ?= $(BASE_DIR)/vpi.tab

CCSRC       := $(wildcard ./*.cc) 
TABS				:= $(patsubst %.cc, %.tab, $(CCSRC))
DOLLAR      := "$$"
default: vpitab

vpitab: $(TABS)
	cat $(TABS) > $(vpitab_file)

$(TABS): %.tab: %.cc
	@echo $(basename $<) call=$(DOLLAR)$(basename $<) acc+=frc:* acc+=rw:* > $@

.PHNOY: clean

clean:
	rm -f $(vpitab_file) *.tab
