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

struct AggregateSampleData {
	enum AggregateSampleMode mode;
	long min, max;
	unsigned avg;
};


/** Get a result based on multiple samples
 *
 *  Instead of just reading the value of the ADC once and using that as a
 *  sample, collect \p samples_n samples and generate a value based on all of
 *  those samples. How the multiple samples are processed to return a single
 *  value is determined by \p mode.
 *
 *  \param channel     The ADC channel to read from
 *  \param samples     An array to store individual samples in
 *  \param result      Option (can be NULL)
 *  \param samples_n   The number of samples to aggregate
 *  \param mode        How to process the samples to a single value
 *
 *  \pre \p samples != NULL
 *  \pre \p samples_n <= the number of values able to be stored in \p samples
 *
 *  \post The values of \p samples will be overwritten with the individual
 *			samples read
 *
 *  \returns
 *			If mode == AGGREGATE_SAMPLE_MAX the maximum value sampled
 *			If mode == AGGREGATE_SAMPLE_AVG the average of \p samples_n
 *				samples (Note: 2 additional samples are taken and discarded:
 *				the first instance of the min and values sample)
 */
unsigned aggregateSample3204(unsigned channel, unsigned *samples,
                             struct AggregateSampleData *result,
                             unsigned samples_n,
                             enum AggregateSampleMode mode)
{
	unsigned i, index;
	long min, max;
	unsigned r;

	min = MCP3204_SAMPLE_MAX + 1;
	max = -1;

	// TODO: Perhaps this function should merely collect the samples in this
	//       function and add an additional "avgAggregateSample" function to
	//       be called by the calling function if desired.

	/* We won't be storing the first occurance of min and mix so add 2 to the
	 * number of iterations
	 */

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

	if (result) {
		result->min = min;
		result->max = max;
	}

	if (mode == AGGREGATE_SAMPLE_MAX)
		r = max;
	else {
		double sum = 0;  // Summing in floating point to avoid overflow issues

		for (i = 0; i < samples_n; i++)
			sum += samples[i];
		r = round(sum / samples_n);
		r &= MCP3204_SAMPLE_MAX;
		if (result)
			result->avg = r;
	}

	return r;
}


int main(void)
{
	int sample;
	unsigned sbuff[100];
	const unsigned sbuff_sz = sizeof sbuff / sizeof sbuff[0];
	double v;

	wiringPiSetup();
	mcp3204Setup(ADC_CHAN_0, 0);

	// Get a first approximation
	sample = aggregateSample3204(ADC_CHAN_0, sbuff, NULL,
					sbuff_sz, AGGREGATE_SAMPLE_AVG);

	v = round(((double)sample / MCP3204_SAMPLE_MAX * VREF) * 100) / 100.0;

	// "Hold"
	for (;;) {
		double v2;
		sample = analogRead(ADC_CHAN_0);
		v2 = round(((double)sample / MCP3204_SAMPLE_MAX * VREF) * 100) / 100.0;
		v = v * 0.999 + v2 * 0.001;
		printf("%.2f      %04x\n", v, sample);
	}

#if 1
	printf("Vref used: %.2f\n",  VREF);
#endif

	return 0;
}
