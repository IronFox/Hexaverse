using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Consensus
{
    public static class LocalTime
    {
		private struct Timer
		{
			public Stopwatch	Watch;

			public Timer(bool start)
			{
				Watch = new Stopwatch();
				if (start)
					Watch.Start();
			}
		}
		private static Timer timer = new Timer(true);

        private static long getFrequency()
        {
			return Stopwatch.Frequency;
        }
        public static readonly long Frequency = getFrequency();
        //public static long Frequency { get { return frequency; } }

        public static readonly double Resolution = 1.0 / (double)getFrequency();
        //public static double Resolution { get { return resolution; } }


        public static double ToSeconds(long ticks)
        {
            return (double)ticks * Resolution;
        }

        public static long Query()
        {
			return timer.Watch.ElapsedTicks;
        }


        public static ulong UnixTime()
        {
            TimeSpan ts = (DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0));
            return (ulong) ts.TotalSeconds;
        }
    }


    public static class NetTime
    {
        static double localToNet = 1000000.0 * LocalTime.Resolution;

        public static long Query()
        {
            long time = LocalTime.Query();
            return (long)((double)time * localToNet);
        }

        public static double ToSeconds(long netTicks)
        {
            return (double)netTicks / 1000000.0;
        }
        public static float ToSecondsf(long netTicks)
        {
            return (float)netTicks / 1000000.0f;
        }
    }


    public static class TimeConversion
    {
        public static long LocalToNetTicks(long ticks)
        {
            return (long)((double)(ticks) * 1000000.0 * LocalTime.Resolution);
        }

        public static long NetToLocalTicks(long mseconds)
        {
            return (long)((double)(mseconds) * (double)(LocalTime.Frequency) / 1000000.0);
        }
    }

}
