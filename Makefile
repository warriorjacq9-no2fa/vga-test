CPP_SRCS = main.cpp display.cpp glad.c
INCLUDES = -I../include

# Compiler/linker flags
LDFLAGS += -lGL -lglfw
CPPFLAGS += $(DEFINES) -Ofast -march=native -flto -Wall -I../include -DVTOP_MODULE=V$(TOP_MODULE)

VERILOG_SRCS?=$(TOP_MODULE)

# Default rule
all: args run

args:
ifeq ($(strip $(TOP_MODULE)),)
	@echo "Error: TOP_MODULE not set." >&2
	@echo "Syntax: $(MAKE) TOP_MODULE=<top module> VERILOG_SRCS=<full path to srcs>" >&2
	@exit 1
endif


# Step 1: Run Verilator to generate C++ model + build Makefile
verilate:
	verilator -Wall --cc --exe $(CPP_SRCS) $(INCLUDES) $(VERILOG_SRCS) $(DEFINES) --bbox-unsup\
		--top-module $(TOP_MODULE) \
		-LDFLAGS "$(LDFLAGS)" -CFLAGS "$(CPPFLAGS)"

# Step 2: Build the executable from generated makefile
build: verilate
	$(MAKE) -j -C obj_dir -f V$(TOP_MODULE).mk V$(TOP_MODULE)

# Step 3: Run the simulation
run: build
	obj_dir/V$(TOP_MODULE)

# Step 4: Cleanup
clean:
	rm -rf obj_dir logs *.vcd *.fst
