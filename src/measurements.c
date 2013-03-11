#define EXINLINED
#include "measurements.h"

#ifdef DO_TIMINGS
ticks entry_time[ENTRY_TIMES_SIZE];
enum timings_bool_t entry_time_valid[ENTRY_TIMES_SIZE] = {M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE, M_FALSE};
ticks total_sum_ticks[ENTRY_TIMES_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
long long total_samples[ENTRY_TIMES_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char *measurement_msgs[ENTRY_TIMES_SIZE];
ticks getticks_correction = 0;

void 
prints_ticks_stats(int start, int end) 
{
  uint32_t i, mpoints = 0, have_output = 0;
  unsigned long long tsamples = 0;
  ticks tticks = 0;

  for (i = start; i < end; i++) 
    {
      if (total_samples[i]) 
	{
	  have_output = 1;
	  mpoints++;
	  tsamples += total_samples[i];
	  tticks += total_sum_ticks[i];
	}
    }
  
  if (have_output)
    {
      printf("(PROFILING) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    }
  for (i = start; i < end; i++) {
    if (total_samples[i] && total_sum_ticks[i]) {
      printf("[%02d]%s:\n", i, measurement_msgs[i]);
      double ticks_perc = 100 * ((double) total_sum_ticks[i] / tticks);
      double secs = total_sum_ticks[i] / (REF_SPEED_GHZ * 1.e9);
      int s = (int) trunc(secs);
      int ms = (int) trunc((secs - s) * 1000);
      int us = (int) trunc(((secs - s) * 1000000) - (ms * 1000));
      int ns = (int) trunc(((secs - s) * 1000000000) - (ms * 1000000) - (us * 1000));
      double secsa = (total_sum_ticks[i] / total_samples[i]) / (REF_SPEED_GHZ * 1.e9);
      int sa = (int) trunc(secsa);
      int msa = (int) trunc((secsa - sa) * 1000);
      int usa = (int) trunc(((secsa - sa) * 1000000) - (msa * 1000));
      int nsa = (int) trunc(((secsa - sa) * 1000000000) - (msa * 1000000) - (usa * 1000));
      printf(" [%4.1f%%] samples: %-12llu | time: %3d %3d %3d %3d | avg: %3d %3d %3d %3d | ticks: %.1f\n",
	     ticks_perc, total_samples[i],
	     s, ms, us, ns,
	     sa, msa, usa, nsa,
	     (double) total_sum_ticks[i]/total_samples[i]);
    }
  }
  if (have_output)
    {
      printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< (PROFILING)\n");
      fflush(stdout);
    }
}

#endif
