from pypapi import papi_high
from pypapi import events as papi_events

# Starts some counters
papi_high.start_counters([
    papi_events.PAPI_FP_OPS,
    papi_events.PAPI_TOT_CYC
])

# Reads values from counters and reset them
results = papi_high.read_counters()  # -> [int, int]

# Reads values from counters and stop them
results = papi_high.stop_counters()  # -> [int, int]
