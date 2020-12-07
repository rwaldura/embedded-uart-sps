/*
 * Main program: reads sensor every minute, outputs values.
 */

#include <stdio.h>
#include <math.h> // roundf
#include <time.h>

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

struct sps30_measurement average_measurements(const struct sps30_measurement mm[], const int n) {
    struct sps30_measurement m = { 0 }; // initialize all fields to zero
    int valid_measurements = 0;

    for (int i = 0; i < n; i++) {
        if (mm[i].typical_particle_size > 0) { // valid measurement
            valid_measurements += 1;
            m.mc_1p0  += mm[i].mc_1p0;
            m.mc_2p5  += mm[i].mc_2p5;
            m.mc_4p0  += mm[i].mc_4p0;
            m.mc_10p0 += mm[i].mc_10p0;
            m.nc_0p5  += mm[i].nc_0p5;
            m.nc_1p0  += mm[i].nc_1p0;
            m.nc_2p5  += mm[i].nc_2p5;
            m.nc_4p0  += mm[i].nc_4p0;
            m.nc_10p0 += mm[i].nc_10p0;
            m.typical_particle_size += mm[i].typical_particle_size;
        }
    }

    if (valid_measurements == 0) {
        m.typical_particle_size = -1;
        return m; // return invalid measurement
    }

    m.mc_1p0  /= valid_measurements;
    m.mc_2p5  /= valid_measurements;
    m.mc_4p0  /= valid_measurements;
    m.mc_10p0 /= valid_measurements;
    m.nc_0p5  /= valid_measurements;
    m.nc_1p0  /= valid_measurements;
    m.nc_2p5  /= valid_measurements;
    m.nc_4p0  /= valid_measurements;
    m.nc_10p0 /= valid_measurements;
    m.typical_particle_size /= valid_measurements;
    return m;
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

    /* stdout to be line-buffered: we print measurement data to a pipe.
     * This is strictly equivalent to: setvbuf(stdout, NULL, _IOLBF, 0) 
     */
    setlinebuf(stdout);

    while (1) {
        ret = sps30_start_measurement();
        if (ret < 0) {
            fprintf(stderr, "error starting measurement\n");
	    break;
        }

        fprintf(stderr, "measurements started\n");

	const int num_samples = 10;
	struct sps30_measurement mm[num_samples];

	fprintf(stderr, "#"
                       "\tpm1.0"
                       "\tpm2.5"
                       "\tpm4.0"
                       "\tpm10.0"
                       "\tnc0.5"
                       "\tnc1.0"
                       "\tnc2.5"
                       "\tnc4.5"
                       "\tnc10.0"
                       "\ttps\n");

        for (int i = 0; i < num_samples; ++i) {
            struct sps30_measurement m;
            ret = sps30_read_measurement(&m);

            if (ret < 0) {
                mm[i].typical_particle_size = -1;
                fprintf(stderr, "error reading measurement #%d\n", i);
            } else if (SPS30_IS_ERR_STATE(ret)) {
                mm[i].typical_particle_size = -1;
                fprintf(stderr,
                    "Chip state: %u - measurement #%d may not be accurate\n",
                    SPS30_GET_ERR_STATE(ret), i);
            } else {
		mm[i] = m;
                fprintf(stderr, "%d"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f"
                       "\t%0.2f\n",
                       i, m.mc_1p0, m.mc_2p5, m.mc_4p0, m.mc_10p0, m.nc_0p5,
                       m.nc_1p0, m.nc_2p5, m.nc_4p0, m.nc_10p0,
                       m.typical_particle_size);
            }
            sensirion_sleep_usec(1 * 1000000); /* sleep for 1s */
        }

	struct sps30_measurement m = average_measurements(mm, num_samples);
	if (m.typical_particle_size > 0) // valid measurement
            printf("%ld" 
               "\t%d"
               "\t%d"
               "\t%d"
               "\t%d"
               "\t%d"
               "\t%d"
               "\t%d"
               "\t%d"
               "\t%d"
               "\t%d\n",
               time(NULL),
	       /* Round all measured values; fractional digits do not carry any valid information.
		* See https://github.com/Sensirion/embedded-uart-sps/issues/77 
		*/
	       _round(m.mc_1p0), _round(m.mc_2p5), _round(m.mc_4p0), _round(m.mc_10p0), _round(m.nc_0p5),
               _round(m.nc_1p0), _round(m.nc_2p5), _round(m.nc_4p0), _round(m.nc_10p0),
               _roundK(m.typical_particle_size));

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
        sensirion_sleep_usec(1000000 * 50);

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
