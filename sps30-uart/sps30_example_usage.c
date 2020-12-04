/*
 * Main program: reads sensor every minute, outputs values.
 */

#include <stdio.h>  // printf
#include <math.h> // roundf

#include "sensirion_uart.h"
#include "sps30.h"

/**
 * TO USE CONSOLE OUTPUT (PRINTF) AND WAIT (SLEEP) PLEASE ADAPT THEM TO YOUR
 * PLATFORM
 */
//#define printf(...)

int _round(float f) {
	return (int) roundf(f);
}

int _roundK(float f) {
	return (int) roundf(1000 * f); // preserve 3 fractional digits
}

int main(int argc, const char* argv[]) {
    char serial[SPS30_MAX_SERIAL_LEN];
    const uint8_t AUTO_CLEAN_DAYS = 4;
    int16_t ret;

    while (sensirion_uart_open() != 0) {
        fprintf(stderr, "UART init failed\n");
        sensirion_sleep_usec(1000000); /* sleep for 1s */
    }

    /* Busy loop for initialization, because the main loop does not work without
     * a sensor.
     */
    while (sps30_probe() != 0) {
        fprintf(stderr, "SPS30 sensor probing failed\n");
        sensirion_sleep_usec(1000000); /* sleep for 1s */
    }
    fprintf(stderr, "SPS30 sensor probing successful\n");

    struct sps30_version_information version_information;
    ret = sps30_read_version(&version_information);
    if (ret) {
        fprintf(stderr, "error %d reading version information\n", ret);
    } else {
        fprintf(stderr, "FW: %u.%u HW: %u, SHDLC: %u.%u\n",
               version_information.firmware_major,
               version_information.firmware_minor,
               version_information.hardware_revision,
               version_information.shdlc_major,
               version_information.shdlc_minor);
    }

    ret = sps30_get_serial(serial);
    if (ret)
        fprintf(stderr, "error %d reading serial\n", ret);
    else
        fprintf(stderr, "SPS30 Serial: %s\n", serial);

    ret = sps30_set_fan_auto_cleaning_interval_days(AUTO_CLEAN_DAYS);
    if (ret)
        fprintf(stderr, "error %d setting the auto-clean interval\n", ret);

    while (1) {
        ret = sps30_start_measurement();
        if (ret < 0) {
            fprintf(stderr, "error starting measurement\n");
        }

        fprintf(stderr, "measurements started\n");

        for (int i = 0; i < 60; ++i) {

		    struct sps30_measurement m;
            ret = sps30_read_measurement(&m);

            if (ret < 0) {
                fprintf(stderr, "error reading measurement\n");
            } else {
                if (SPS30_IS_ERR_STATE(ret)) {
                    fprintf(stderr,
                        "Chip state: %u - measurements may not be accurate\n",
                        SPS30_GET_ERR_STATE(ret));
                }

                printf("measured values:\n"
                       "\t%d pm1.0\n"
                       "\t%d pm2.5\n"
                       "\t%d pm4.0\n"
                       "\t%d pm10.0\n"
                       "\t%d nc0.5\n"
                       "\t%d nc1.0\n"
                       "\t%d nc2.5\n"
                       "\t%d nc4.5\n"
                       "\t%d nc10.0\n"
                       "\t%d typical particle size\n\n",
                       _round(m.mc_1p0), _round(m.mc_2p5), _round(m.mc_4p0), _round(m.mc_10p0), _round(m.nc_0p5),
                       _round(m.nc_1p0), _round(m.nc_2p5), _round(m.nc_4p0), _round(m.nc_10p0),
                       _roundK(m.typical_particle_size));
            }
            sensirion_sleep_usec(1000000); /* sleep for 1s */
        }

        /* Stop measurement for 1min to preserve power. Also enter sleep mode
         * if the firmware version is >=2.0.
         */
        ret = sps30_stop_measurement();
        if (ret) {
            fprintf(stderr, "Stopping measurement failed\n");
        }

        if (version_information.firmware_major >= 2) {
            ret = sps30_sleep();
            if (ret) {
                fprintf(stderr, "Entering sleep failed\n");
            }
        }

        fprintf(stderr, "No measurements for 1 minute\n");
        sensirion_sleep_usec(1000000 * 60);

        if (version_information.firmware_major >= 2) {
            ret = sps30_wake_up();
            if (ret) {
                fprintf(stderr, "Error %i waking up sensor\n", ret);
            }
        }
    }

    if (sensirion_uart_close() != 0)
        fprintf(stderr, "failed to close UART\n");

    return 0;
}
