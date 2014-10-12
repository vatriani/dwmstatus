FLAGS:=-pipe -fstack-protector -O5 -fdata-sections -ffunction-sections -Wl,--gc-sections --param=ssp-buffer-size=4 -fexcess-precision=fast
TARGET=dwmstatus

BATPATH=`find /sys/class/power_supply/ -name BAT1 -print0 -quit`
LNKPATH=`find /sys/class/net/wlp3s0/ -name operstate -print0 -quit`
LAPATH=`find /proc -name stat -print0 -quit`
CPUFREQ=`find /sys/devices/system/cpu/cpu0/cpufreq/ -name scaling_cur_freq -print0 -quit`
BOXSUSPEND=`which boxsuspend`

all: $(TARGET)

build_host.h:
	@echo "#define BUILD_HOST \"`hostname`\""  > build_host.h
	@echo "#define BUILD_OS \"`uname`\""          >> build_host.h
	@echo "#define BUILD_PLATFORM \"`uname -m`\"" >> build_host.h
	@echo "#define BUILD_KERNEL \"`uname -r`\""   >> build_host.h
	@echo "#define LA_PATH \"${LAPATH}\""  >> build_host.h
	@echo "#define BAT_NOW \"${BATPATH}/charge_now\""  >> build_host.h
	@echo "#define BAT_FULL \"${BATPATH}/charge_full\""  >> build_host.h
	@echo "#define BAT_STAT \"${BATPATH}/status\""  >> build_host.h
	@echo "#define LNK_PATH \"${LNKPATH}\"" >> build_host.h
	@echo "#define BOX_SUSPEND \"${BOXSUSPEND}\"" >> build_host.h
	@echo "#define FREQ_STAT \"${CPUFREQ}\"" >> build_host.h

dwmstatus: build_host.h main.c
	cc $(FLAGS) -mtune=atom -march=atom main.c -o $(TARGET) -lX11 -L/usr/lib 

clean:
	rm -f *.d *.o $(TARGET) build_host.h

install: $(TARGET)
	cp $(TARGET) /usr/bin/$(TARGET)
	chmod go+rx /usr/bin/$(TARGET)
