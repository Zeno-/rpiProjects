#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>


/* wiring Pi library
 */
#include <wiringPi.h>
#include <mcp3208.h>

#define LED_PIN                  0

#define AGGREGATE_SAMPLE_REPEATS 256

#define PIN_BASE                 100

#define VREF                     4.096
#define MCP3204_SAMPLE_MAX       0xFFF

#define ADC_CHAN_0               (PIN_BASE)
#define ADC_CHAN_1               (ADC_CHAN_0 + 1)

#define SENSOR_90_DEGREES        0
#define SENSOR_TRANSMITTED       1

#define OUTSCALE_UPPER_LIMIT     100

/**************************************************************************
 **************************************************************************/

struct AggregateSampleData {
	unsigned min, max, avg, median, mode;
};

/**************************************************************************
 **************************************************************************/
double takeSample(unsigned adc_channel);

void aggregateSample3204(unsigned channel, unsigned *samples,
                             struct AggregateSampleData *result,
                             unsigned samples_n);

double round_decimal_places(double value, unsigned places);

/**************************************************************************
 **************************************************************************/

int main(void)
{
	double dark_volts_90, dark_volts_transmitted, light_volts;
	double sample_volts, sample_90, sample_transmitted;
	double c;

	/* Setup wiringPi
	 */
	wiringPiSetup();
	mcp3208Setup(ADC_CHAN_0, 0);
	pinMode(LED_PIN, OUTPUT);

#if SELF_CALIBRATE
	digitalWrite (LED_PIN, 1);    // Turn LED on
	delay(250);
	printf("Taking light_volts sample...\n");
	light_volts = takeSample(SENSOR_90_DEGREES);
	light_volts = round_decimal_places(light_volts, 3);
	digitalWrite(0, 0);
#else
	light_volts = VREF;
#endif

	// TODO: Ensure that light_volts > dark_volts (avoids potential div-by-zero below also)

	digitalWrite(LED_PIN, 0);	// Make sure LED is off
	delay(250);
	printf("Taking dark_volts sample...\n");

	dark_volts_90 = takeSample(SENSOR_90_DEGREES);
	dark_volts_90 = round_decimal_places(dark_volts_90, 3);
	printf("90 degree dark voltage: %f\n", dark_volts_90);

	dark_volts_transmitted = takeSample(SENSOR_TRANSMITTED);
	dark_volts_transmitted = round_decimal_places(dark_volts_transmitted, 3);
	printf("transmitted dark voltage: %f\n", dark_volts_transmitted);


	printf("Taking sample...\n");

#if 1
	/* Read transmitted light sensor
	 */
	digitalWrite (LED_PIN, 1);
	delay(250);
	sample_volts = takeSample(SENSOR_TRANSMITTED);
	sample_volts = round_decimal_places(sample_volts, 3);
	digitalWrite(LED_PIN, 0);

	sample_volts -= dark_volts_90;		// FIXME: Use correct dark voltage
	if (sample_volts < 0)
		sample_volts = 0;
	printf("transmitted sample voltage = %f\n", sample_volts);

	sample_transmitted = sample_volts;
#endif

	/* Read 90 degree light sensor
	 */
	digitalWrite (LED_PIN, 1);
	delay(250);
	sample_volts = takeSample(SENSOR_90_DEGREES);
	sample_volts = round_decimal_places(sample_volts, 3);
	digitalWrite(LED_PIN, 0);

	sample_volts -= dark_volts_90;
	if (sample_volts < 0)
		sample_volts = 0;
	printf("90 degree sample voltage = %f\n", sample_volts);

	sample_90 = sample_volts;

	/* Calculate and output
	 */
	sample_90 /= (light_volts - dark_volts_90);

	sample_90 *= OUTSCALE_UPPER_LIMIT;
	if (sample_90 > OUTSCALE_UPPER_LIMIT)
		sample_90 = OUTSCALE_UPPER_LIMIT;

	printf("---- Sample (clarity 0 - 100.0) = %.3f\n", sample_90);

	return 0;
}

double takeSample(unsigned adc_channel)
{
	struct AggregateSampleData as_result;
	static unsigned sbuff[AGGREGATE_SAMPLE_REPEATS];
	const unsigned sbuff_sz = sizeof sbuff / sizeof sbuff[0];
	unsigned i;

#ifdef VERBOSE_OUT
	int vcount = 0;
	double v;
	double var_sum;
	double std_dev;
	unsigned j, nzvalues;
#endif

	unsigned hist[MCP3204_SAMPLE_MAX + 1];

	aggregateSample3204(PIN_BASE + adc_channel, sbuff, &as_result, sbuff_sz);

	memset(hist, 0, sizeof hist);

	as_result.mode = 0;
	for (i = 0; i < sizeof hist / sizeof hist[0]; i++) {
		if (hist[i] != 0 && hist[i] > as_result.mode) {
			as_result.mode = i;
		}
	}

#ifdef VERBOSE_OUT

	// Standard deviation
	var_sum = 0;

	for (i = 0; i < sbuff_sz; i++) {
		long delta;
		hist[sbuff[i]]++;
		delta = sbuff[i] - as_result.avg;
		var_sum += delta * delta;
	}

	std_dev = sqrt(var_sum / sbuff_sz);


	/* Count unique zon-zero values
	 */
	nzvalues = 0;
	for (i = 0; i < sizeof hist / sizeof hist[0]; i++)
		nzvalues += hist[i] != 0;

	v = (double)as_result.avg / MCP3204_SAMPLE_MAX * VREF;
	printf("%% (mean):                  %.3f\n", v);

	v = (double)as_result.median / MCP3204_SAMPLE_MAX * VREF;
	printf("%% (median):                %.3f\n", v);

	v = (double)as_result.mode / MCP3204_SAMPLE_MAX * VREF;
	printf("%% (mode):                  %.3f\n", v);

	v = ((double)as_result.mode + as_result.median) / 2.0 / MCP3204_SAMPLE_MAX * VREF;
	printf("%% (mode/median avg):       %.3f\n", v);

	v = std_dev / MCP3204_SAMPLE_MAX * VREF;
	printf("%% (std dev):               %.3f\n", v);

	v = (std_dev / sqrt(sbuff_sz)) / MCP3204_SAMPLE_MAX * VREF;
	printf("%% (std err):               %.3f\n", v);

	printf("Total samples:             %u\n", sbuff_sz);
	printf("Unique values sampled:     %u\n", nzvalues);

	v = (double)as_result.min / MCP3204_SAMPLE_MAX * VREF;
	printf("    min:                   %.4f\n", v);

	v = (double)as_result.max / MCP3204_SAMPLE_MAX * VREF;
	printf("    max:                   %.4f\n", v);
#endif

	return (double)as_result.median / MCP3204_SAMPLE_MAX * VREF;
}



static int cmp_unsigned(const void *a, const void *b)
{
	const unsigned *ua, *ub;

	ua = a;
	ub = b;

	return *ua - *ub;
}

void aggregateSample3204(unsigned channel, unsigned *samples,
                             struct AggregateSampleData *result,
                             unsigned samples_n)
{
	unsigned i;
	long min, max;
	double avg;
	unsigned middle = samples_n / 2;

	min = MCP3204_SAMPLE_MAX + 1;
	max = 0;

	// precondition: samples_n >= 2

	avg = 0;
	for (i = 0; i < samples_n; i++) {
		unsigned sample= analogRead(channel);
		if (sample > max) {
			max = sample;
		} else if (sample < min) {
			min = sample;
		}

		samples[i] = sample;

		avg += sample;

		delay(1);	// FIXME: Implement a proper timing system
	}

	// TODO: The median should be calculated by the calling function rather
	// than done here especially since, unlike min, max and average, we cannot
	// do it "inline" with taking the samples.
	qsort(samples, samples_n, sizeof *samples, cmp_unsigned);

	result->min = min;
	result->max = max;
	result->avg = avg / samples_n;

	if (samples_n & 0x01) { // If odd number
		result->median = (samples[middle - 1] + samples[middle]) / 2;
	} else {
		result->median = samples[middle];
	}
}

double round_decimal_places(double value, unsigned places)
{
	double multiplier;

	multiplier = pow(10, places);

	return ceil(value * multiplier + 0.5) / multiplier;
}

