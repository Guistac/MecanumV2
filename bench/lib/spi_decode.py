"""Decode SPI reads from raw scope data (SCK + MISO, no chip select wire).

RadioHead talks to the RFM23BP in SPI mode 0 (data sampled on SCK rising edges, about 1 MHz).
A register access is 16 clocks: the address byte (MISO undefined) then the data byte. Without nSEL
the frames are separated by gaps in the clock: edges further apart than gap_s start a new frame.
"""

THRESHOLD_V = 2.0       # 5 V side of the TXB0108: SCK and MISO swing 0 to about 4 V


def frames(sck, miso, gap_s=1.7e-6):
    """sck, miso = (t_first_s, dt_s, volts) -> list of {t_s, edges, bytes}.
    gap_s validated 2026-10-26: 1.5 us between the two bytes of an access, about 1.9 us between
    accesses at RadioHead's 1 MHz SPI clock."""
    t0, dt, vs = sck
    _, _, vm = miso
    edges = [i for i in range(1, len(vs)) if vs[i - 1] < THRESHOLD_V <= vs[i]]
    out, current = [], []
    for k, i in enumerate(edges):
        if current and (i - current[-1]) * dt > gap_s:
            out.append(current)
            current = []
        current.append(i)
    if current:
        out.append(current)
    result = []
    for f in out:
        bits = [1 if vm[i] > THRESHOLD_V else 0 for i in f]
        data = []
        for b in range(0, len(bits) - len(bits) % 8, 8):
            value = 0
            for bit in bits[b:b + 8]:
                value = (value << 1) | bit
            data.append(value)
        result.append({"t_s": t0 + f[0] * dt, "edges": len(f), "bytes": data})
    return result


def low_intervals(signal, threshold_v=1.65, min_s=0.0):
    """-> list of (start_s, length_s) where the signal is below threshold"""
    t0, dt, v = signal
    out, start = [], None
    for i, x in enumerate(v):
        if x < threshold_v and start is None:
            start = i
        elif x >= threshold_v and start is not None:
            if (i - start) * dt >= min_s:
                out.append((t0 + start * dt, (i - start) * dt))
            start = None
    if start is not None:
        out.append((t0 + start * dt, (len(v) - start) * dt))
    return out


def summarize(sig, chip_ready_bit=0x02):
    """Summary of a T6e capture: the frames after the long quiet gap of RHSPIDriver::init()
    (software reset #1, device type read, software reset #2, then the chip ready polls)."""
    fr = frames(sig["SCK"], sig["MISO"])
    data_bytes = [f["bytes"][1] if len(f["bytes"]) >= 2 else None for f in fr]
    nirq_low = low_intervals(sig["NIRQ"], min_s=200e-9)
    polls = [f for f in fr[3:] if f["edges"] == 16 and len(f["bytes"]) == 2]
    first_ready = next((f["t_s"] for f in polls if f["bytes"][1] & chip_ready_bit), None)
    counts = {}
    for f in polls:
        counts["0x%02X" % f["bytes"][1]] = counts.get("0x%02X" % f["bytes"][1], 0) + 1
    return {
        "frames": len(fr),
        "frame_edges": sorted(set(f["edges"] for f in fr)),
        "first_frames": [(round(f["t_s"] * 1e3, 4), f["edges"], ["0x%02X" % b for b in f["bytes"]]) for f in fr[:6]],
        "poll_values_after_frame3": counts,
        "first_ready_in_miso_ms": round(first_ready * 1e3, 4) if first_ready is not None else None,
        "nirq_low": [(round(s * 1e3, 4), round(l * 1e6, 1)) for s, l in nirq_low[:10]],
        "nirq_low_count": len(nirq_low),
        "device_type_byte": "0x%02X" % data_bytes[1] if len(data_bytes) > 1 and data_bytes[1] is not None else None,
    }
