#include <stdio.h>
#include <time.h>
#include <math.h>


/* wiring Pi library
 */
#include <wiringPi.h>
#include <mcp3204.h>


#define REPEAT_NUM 10000

#define PIN_BASE                 100

#define MCP3204_SAMPLE_MAX       0xFFF

#define ADC_CHAN_0               (PIN_BASE)
#define ADC_CHAN_1               (ADC_CHAN_0 + 1)

#define VREF                     5.0

enum AggregateSampleMode {
	AGGREGATE_SAMPLE_MAX,
	AGGREGATE_SAMPLE_AVG
};

unsigned aggregateSample3204(unsigned channel, unsigned *samples, unsigned samples_n, enum AggregateSampleMode mode)
{
	unsigned i, index;
	int min, max;
	unsigned result;

	min = MCP3204_SAMPLE_MAX + 1;
	max = -1;

	/* We won't be storing the first occurance of min and mix so add 2 to the
	 * number of iterations
	 */

	// TODO: Since we're determining min and max regardless of the mode we
	//       should somehow return them to the calling function

	// TODO: Perhaps this function should merely collect the samples in this
	//       function and add an additional "avgAggregateSample" function to
	//       be called by the calling function if desired. This current function
	//       would then return nothing (except maybe a min/max struct if it's not
	//       passed as a pointer)

	index = 0;	// Array index
	for (i = 0; i < samples_n + 2; i++) {
		unsigned sample = analogRead(channel);
		if (sample > max) {
			if (max > -1)
				samples[index++] = max;
			max = sample;
		} else if (sample < min) {
			if (min < MCP3204_SAMPLE_MAX + 1)
				samples[index++] = min;
			min = sample;
		} else {
			samples[index++] = sample;
		}
	}

	if (mode == AGGREGATE_SAMPLE_MAX)
		result = max;
	else {
		double sum = 0;  // Summing in floating point to avoid overflow issues

		for (i = 0; i < samples_n; i++)
			sum += samples[i];
		result = round(sum / samples_n);
		result &= MCP3204_SAMPLE_MAX;
	}

	return result;
}


int main(void)
{
	int i, j;
	int sample;
	unsigned sbuff[100];
	const unsigned sbuff_sz = sizeof sbuff / sizeof sbuff[0];
	double v;

	wiringPiSetup();
    mcp3204Setup(ADC_CHAN_0, 0);

	// Get a first approximation
	sample = aggregateSample3204(ADC_CHAN_0, sbuff, sbuff_sz, AGGREGATE_SAMPLE_AVG);

	v = round(((double)sample / MCP3204_SAMPLE_MAX * VREF) * 100) / 100.0;

	// "Hold"
	//for (i = 0; i < REPEAT_NUM; i++) {
	for (;;) {
		double v2, vt;
		sample = analogRead(ADC_CHAN_0);
		v2 = round(((double)sample / MCP3204_SAMPLE_MAX * VREF) * 100) / 100.0;
		vt = v * 0.999 + v2 * 0.001;
		//if (fabs(vt - v) > 0.0025)
			v = vt;
		printf("%.2f      %04x\n", v, sample);
	}

#if 1
	printf("Vref was %.2f\n",  VREF);
#endif

	return 0;
}

