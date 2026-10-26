"""Turn the radio control lines into a state timeline.

TX_ON / RX_ON are active low and driven by the RFM23BP's own GPIOs (high level 3.3 V), see
docs/radio/01-issues-and-root-causes.md:
    TX_ON low,  RX_ON high -> TX        (PA on, driven)
    TX_ON high, RX_ON low  -> RX        (LNA on)
    both high              -> OFF       (PA and LNA off)
    both low               -> BOTH_LOW  (PA enabled without drive)
SDN high (5 V side) means the chip is in shutdown and its GPIOs are not driven.
"""

CTRL_THRESHOLD_V = 1.65
SDN_THRESHOLD_V = 2.5


def state_at(tx_on, rx_on, sdn):
    if sdn > SDN_THRESHOLD_V:
        return "SHUTDOWN"
    tx_high, rx_high = tx_on > CTRL_THRESHOLD_V, rx_on > CTRL_THRESHOLD_V
    return {(False, True): "TX", (True, False): "RX", (True, True): "OFF", (False, False): "BOTH_LOW"}[(tx_high, rx_high)]


def timeline(signals):
    """signals from capture.load_capture -> list of (state, t_start_s, duration_s), relative to trigger."""
    t0, dt, tx = signals["TX_ON"]
    _, _, rx = signals["RX_ON"]
    _, _, sdn = signals["SDN"]
    states = [state_at(a, b, c) for a, b, c in zip(tx, rx, sdn)]
    segments, start = [], 0
    for i in range(1, len(states) + 1):
        if i == len(states) or states[i] != states[start]:
            segments.append((states[start], t0 + start * dt, (i - start) * dt))
            start = i
    return segments


def merge_glitches(segments, min_s):
    """Fold segments shorter than min_s (noise on a slow edge) into the previous one."""
    merged = []
    for seg in segments:
        if merged and (seg[2] < min_s or seg[0] == merged[-1][0]):
            state, start, length = merged[-1]
            merged[-1] = (state, start, length + seg[2])
        else:
            merged.append(seg)
    return merged


def first_crossing(signal, level, rising=True):
    """time of the first crossing of level, or None"""
    t0, dt, v = signal
    for i in range(1, len(v)):
        if (rising and v[i - 1] < level <= v[i]) or (not rising and v[i - 1] > level >= v[i]):
            return t0 + i * dt
    return None
