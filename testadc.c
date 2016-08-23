#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>


/* wiring Pi library
 */
#include <wiringPi.h>
#include <mcp3208.h>


#define REPEAT_NUM 10000

#define PIN_BASE                 100

#define MCP3204_SAMPLE_MAX       0xFFF

#define ADC_CHAN_0               (PIN_BASE)
#define ADC_CHAN_1               (ADC_CHAN_0 + 1)

#define VREF                     4.096

struct AggregateSampleData {
	unsigned min, max, avg, median, mode;
};


int cmp_unsigned(const void *a, const void *b)
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
	unsigned avg;
	unsigned middle = samples_n / 2;

	min = MCP3204_SAMPLE_MAX + 1;
	max = -1;

	// precondition: samples_n >= 2

	avg = 0;
	for (i = 0; i < samples_n; i++) {
		unsigned sample = analogRead(channel);
		if (sample > max) {
			max = sample;
		} else if (sample < min) {
			min = sample;
		}

		samples[i] = sample;

		avg = avg * 0.99 + sample * 0.01;

		//printf("%04x\n", sample);

		//delay(1);	// FIXME: Implement a proper timing system
	}

	qsort(samples, samples_n, sizeof *samples, cmp_unsigned);

	result->min = min;
	result->max = max;
	result->avg = avg;

	if (samples_n & 0x01) { // If odd number
		result->median = (samples[middle - 1] + samples[middle]) / 2;
	} else {
		result->median = samples[middle];
	}
}


int main(void)
{
	struct AggregateSampleData as_result;
	int vcount = 0;
	unsigned sbuff[2048];
	const unsigned sbuff_sz = sizeof sbuff / sizeof sbuff[0];
	double v;
	unsigned i;
	double var_sum;
	double std_dev;

	unsigned hist[MCP3204_SAMPLE_MAX];

	wiringPiSetup();
	mcp3208Setup(ADC_CHAN_0, 0);


#if 1
	for (;;) {
#endif
	aggregateSample3204(ADC_CHAN_0, sbuff, &as_result, sbuff_sz);

	memset(hist, 0, sizeof hist);

	var_sum = 0;

	for (i = 0; i < sbuff_sz; i++) {
		long delta;
		hist[sbuff[i]]++;
		delta = sbuff[i] - as_result.avg;
		var_sum += delta * delta;
	}

	std_dev = sqrt(var_sum / sbuff_sz);

	as_result.mode = 0;
	for (i = 0; i < sizeof hist / sizeof hist[0]; i++) {
		if (hist[i] != 0 && hist[i] > as_result.mode) {
			as_result.mode = i;
		}
	}

#if 0
	v = round(((double)as_result.avg / MCP3204_SAMPLE_MAX * VREF) * 1000) / 1000.0;
	printf("Volts (mean):              %.3f\n", v);

	v = round(((double)as_result.median / MCP3204_SAMPLE_MAX * VREF) * 1000) / 1000.0;
	printf("Volts (median):            %.3f\n", v);

	v = round(((double)as_result.mode / MCP3204_SAMPLE_MAX * VREF) * 1000) / 1000.0;
	printf("Volts (mode):              %.3f\n", v);

	v = round(( ((double)as_result.mode + as_result.median) / 2.0 / MCP3204_SAMPLE_MAX * VREF) * 1000) / 1000.0;
	printf("Volts (mode/median avg):   %.3f\n", v);

	v = round((std_dev / MCP3204_SAMPLE_MAX * VREF) * 1000) / 1000.0;
	printf("Volts (std dev):           %.3f\n", v);

	v = round(((std_dev / sqrt(sbuff_sz)) / MCP3204_SAMPLE_MAX * VREF) * 1000) / 1000.0;
	printf("Volts (std err):           %.3f\n", v);
#else
	v = round(( ((double)as_result.mode + as_result.median) / 2.0 / MCP3204_SAMPLE_MAX * VREF) * 1000) / 1000.0;
	printf("Volts (mode/median avg):   %.3f\n", v);
#endif

#if 1
	}
#endif

#if 1
	printf("Vref used: %.3f\n",  VREF);
#endif

	return 0;
}
