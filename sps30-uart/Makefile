# See user_config.inc for build customizations
-include user_config.inc
include default_config.inc

.PHONY: all clean

all: sps30_example_usage

sps30_example_usage: clean
	$(CC) $(CFLAGS) -c ${sps30_uart_sources} ${uart_sources} ${sps30_uart_dir}/sps30_example_usage.c
	$(CC) $(LDLIBS) -o $@ *.o

clean:
	$(RM) sps30_example_usage
	$(RM) *.o *.gch
