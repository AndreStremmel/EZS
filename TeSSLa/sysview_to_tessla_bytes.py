#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sysview_to_tessla_bytes.py
  Converts a SEGGER SystemView CSV export into the TeSSLa interpreter's
  input format:  <timestamp_ns>: <stream> = <value>

  WHY THIS VARIANT EXISTS:
  On several recordings, SystemView failed to reliably resolve our own
  module's event names (e.g. "MtxLockOk") and kept showing
  "Function #NNN" instead - even though the full module description had
  been transmitted. The root cause was not conclusively identified. To
  make verification independent of that, this script decodes the
  parameters directly from the raw bytes in the "eventdata" column
  (VarUint32 encoding, as used internally by SystemView) - plain-text
  names are NOT required for this to work.

  Per-event byte layout (calibrated against real recordings):
    VarUint32[0]   = function number (EventOffset + enum index)
    VarUint32[1]   = number of payload parameters (2 or 3, depending on
                     the event)
    VarUint32[2..] = the actual parameters, in the order they were
                     passed to OS_Trace_RecordU32x2/x3 in the firmware
                     (e.g. Mtx, Task)
    last VarUint32 = a SystemView-internal timestamp delta (ignored -
                     the row's own timestampint column is the
                     authoritative source)

  Pipeline:
    1. SystemView: start recording, run the scenario, stop
    2. Events window -> Export as CSV
    3. python3 sysview_to_tessla_bytes.py trace_export.csv -o trace.input
    4. java -jar tessla.jar interpreter spec.tessla trace.input
"""

import argparse
import csv
import re

# ============================================================================
# CONFIGURATION
# ============================================================================

# Both tasksets - the INDEX is what matters, not the name.
# Normal operation (OS_RUN_INTEGRATION_TESTS = 0) and test mode (= 1)
# occupy the same four slots, so both name families are listed here:
#   Slot 0 = Sensor    / TestHigh   (prio 3)
#   Slot 1 = Proc      / TestMain   (prio 1)
#   Slot 2 = UartShell / TestPeer   (prio 1)
#   Slot 3 = Idle      / Idle       (prio 0)
TASK_NAME_TO_IDX = {
    "Sensor":    0,
    "Proc":      1,
    "UartShell": 2,
    "Idle":      3,
    "TestHigh":  0,
    "TestMain":  1,
    "TestPeer":  2,
}

MTX_NAMES = {1: "uart", 2: "cfg"}
SEM_NAMES = {1: "echo", 2: "uartrx"}
Q_NAMES   = {1: "sensor", 2: "proc"}

OS_TRACE_EVT_BASE = 512

# Event index (as in the enum in os_trace.h) -> (plain-text name,
# parameter count). Order MUST match the enum!
MODULE_EVENT_TABLE = [
    ("MtxLockTry",      2),
    ("MtxLockOk",       2),
    ("MtxLockBlock",    3),
    ("MtxLockTimeout",  2),
    ("MtxUnlockOk",     2),
    ("MtxUnlockDenied", 2),
    ("SemTakeTry",      2),
    ("SemTakeOk",       3),
    ("SemTakeBlock",    3),
    ("SemTakeTimeout",  2),
    ("SemGiveOk",       3),
    ("SemGiveIgnored",  2),
    ("QSendTry",        2),
    ("QSendOk",         3),
    ("QSendFull",       2),
    ("QSendBlock",      3),
    ("QSendTimeout",    2),
    ("QRecvTry",        2),
    ("QRecvOk",         3),
    ("QRecvEmpty",      2),
    ("QRecvBlock",      3),
    ("QRecvTimeout",    2),
    ("DelayStart",      2),
    ("BusyDelayStart",  2),
    ("BusyDelayEnd",    1),
    ("TaskMap",         2),
    ("UartTxDist",      1),
    ("SensorErr",       1),
    ("CalStart",        1),
    ("CalDone",         1),
]
FUNCTION_NUM_TO_NAME = {
    OS_TRACE_EVT_BASE + i: name for i, (name, _) in enumerate(MODULE_EVENT_TABLE)
}
FUNCTION_NUM_TO_NPARAMS = {
    OS_TRACE_EVT_BASE + i: n for i, (_, n) in enumerate(MODULE_EVENT_TABLE)
}

# Event name -> parameter field names, in the order the firmware passes
# them to OS_Trace_RecordU32x2/x3 (see os_mutex.c/os_semaphore.c/
# os_queue.c/scheduler.c/tasks.c) - needed to attach the right key=value
# names to the decoded numbers, as expected by MODULE_EVENTS below.
PARAM_NAMES = {
    "MtxLockTry":      ["Mtx", "Task"],
    "MtxLockOk":       ["Mtx", "Task"],
    "MtxLockBlock":    ["Mtx", "Task", "Ticks"],
    "MtxLockTimeout":  ["Mtx", "Task"],
    "MtxUnlockOk":     ["Mtx", "Task"],
    "MtxUnlockDenied": ["Mtx", "Task"],
    "SemTakeTry":      ["Sem", "Task"],
    "SemTakeOk":       ["Sem", "Task", "Cnt"],
    "SemTakeBlock":    ["Sem", "Task", "Ticks"],
    "SemTakeTimeout":  ["Sem", "Task"],
    "SemGiveOk":       ["Sem", "Task", "Cnt"],
    "SemGiveIgnored":  ["Sem", "Task"],
    "QSendTry":        ["Q", "Task"],
    "QSendOk":         ["Q", "Chk", "Cnt"],
    "QSendFull":       ["Q", "Task"],
    "QSendBlock":      ["Q", "Task", "Ticks"],
    "QSendTimeout":    ["Q", "Task"],
    "QRecvTry":        ["Q", "Task"],
    "QRecvOk":         ["Q", "Chk", "Cnt"],
    "QRecvEmpty":      ["Q", "Task"],
    "QRecvBlock":      ["Q", "Task", "Ticks"],
    "QRecvTimeout":    ["Q", "Task"],
    "DelayStart":      ["Task", "Ticks"],
    "BusyDelayStart":  ["Task", "Ticks"],
    "BusyDelayEnd":    ["Task"],
    "TaskMap":         ["Idx", "Addr"],
    "UartTxDist":      ["Dist"],
    "SensorErr":       ["Code"],
    "CalStart":        ["Target"],
    "CalDone":         ["Offset"],
}


def _mtx(kind):
    def emit(p):
        m = MTX_NAMES.get(p.get("Mtx", -1))
        if m is None:
            return []
        out = [(f"mtx_{m}_{kind}", p.get("Task", 0))]
        if kind == "block" and "Ticks" in p:
            # One stream per task: otherwise the spec would have to
            # re-join Ticks and Task via filter/last, which systematically
            # mis-attributes durations once multiple tasks' blocking
            # calls interleave.
            out.append((f"mtx_{m}_block_ticks_{p.get('Task', 0)}", _ticks(p["Ticks"])))
        return out
    return emit

def _sem(kind, with_count=False):
    def emit(p):
        s = SEM_NAMES.get(p.get("Sem", -1))
        if s is None:
            return []
        out = [(f"sem_{s}_{kind}", p.get("Task", 0))]
        if with_count and "Cnt" in p:
            out.append((f"sem_{s}_count", p["Cnt"]))
        if kind == "block" and "Ticks" in p:
            out.append((f"sem_{s}_block_ticks_{p.get('Task', 0)}", _ticks(p["Ticks"])))
        return out
    return emit

def _q(kind, chk=False):
    def emit(p):
        q = Q_NAMES.get(p.get("Q", -1))
        if q is None:
            return []
        out = []
        if chk:
            out.append((f"q_{q}_{kind}", p.get("Chk", 0)))
            if "Cnt" in p:
                out.append((f"q_{q}_count", p["Cnt"]))
        else:
            out.append((f"q_{q}_{kind}", p.get("Task", 0)))
            if kind.endswith("block") and "Ticks" in p:
                out.append((f"q_{q}_{kind}_ticks_{p.get('Task', 0)}", _ticks(p["Ticks"])))
        return out
    return emit

def _ticks(v):
    return -1 if v == 0xFFFFFFFF else v

MODULE_EVENTS = {
    "MtxLockTry":      _mtx("try"),
    "MtxLockOk":       _mtx("ok"),
    "MtxLockBlock":    _mtx("block"),
    "MtxLockTimeout":  _mtx("timeout"),
    "MtxUnlockOk":     _mtx("unlock"),
    "MtxUnlockDenied": _mtx("unlock_denied"),
    "SemTakeTry":      _sem("try"),
    "SemTakeOk":       _sem("ok", with_count=True),
    "SemTakeBlock":    _sem("block"),
    "SemTakeTimeout":  _sem("timeout"),
    "SemGiveOk":       _sem("give", with_count=True),
    "SemGiveIgnored":  _sem("give_ignored"),
    "QSendTry":        _q("send_try"),
    "QSendOk":         _q("send_ok", chk=True),
    "QSendFull":       _q("send_full"),
    "QSendBlock":      _q("send_block"),
    "QSendTimeout":    _q("send_timeout"),
    "QRecvTry":        _q("recv_try"),
    "QRecvOk":         _q("recv_ok", chk=True),
    "QRecvEmpty":      _q("recv_empty"),
    "QRecvBlock":      _q("recv_block"),
    "QRecvTimeout":    _q("recv_timeout"),
    # Delays: in addition to the collective stream (delay_task), one
    # ticks stream PER TASK. Specs can then read delay_ticks_<i> directly
    # instead of re-joining task and ticks via filter/last.
    "DelayStart":      lambda p: [("delay_task",  p.get("Task", 0)),
                                  (f"delay_ticks_{p.get('Task', 0)}", p.get("Ticks", 0))],
    "BusyDelayStart":  lambda p: [("busy_task",   p.get("Task", 0)),
                                  (f"busy_ticks_{p.get('Task', 0)}", p.get("Ticks", 0))],
    "BusyDelayEnd":    lambda p: [("busy_end",    p.get("Task", 0))],
    "TaskMap":         None,
    "UartTxDist":      lambda p: [("uart_tx_dist", p.get("Dist", 0))],
    "SensorErr":       lambda p: [("sensor_err",   p.get("Code", 0))],
    "CalStart":        lambda p: [("cal_start",    p.get("Target", 0))],
    "CalDone":         lambda p: [("cal_done",     p.get("Offset", 0))],
}

# ============================================================================
# VarUint32 decoding of the eventdata column
# ============================================================================

def decode_varuint32_stream(data_bytes):
    """Decodes a sequence of LEB128/VarUint32 values from raw bytes."""
    vals = []
    i = 0
    n = len(data_bytes)
    while i < n:
        val = 0
        shift = 0
        while i < n:
            b = data_bytes[i]
            i += 1
            val |= (b & 0x7F) << shift
            shift += 7
            if (b & 0x80) == 0:
                break
        vals.append(val)
    return vals


def parse_eventdata_hex(hexstr):
    """'81 04 02 02 01 FB 08' -> [0x81, 0x04, ...] (robust against
    surrounding whitespace)."""
    toks = hexstr.strip().split()
    out = []
    for t in toks:
        try:
            out.append(int(t, 16))
        except ValueError:
            continue
    return out


def resolve_via_bytes(func_num, eventdata_hex):
    """Fallback: function number + raw bytes -> (event name, parameter
    dict). Returns (None, {}) if func_num is not a known module event,
    or the decoded values don't match the expected parameter count."""
    name = FUNCTION_NUM_TO_NAME.get(func_num)
    if name is None:
        return None, {}
    nparams = FUNCTION_NUM_TO_NPARAMS[func_num]

    raw = parse_eventdata_hex(eventdata_hex)
    vals = decode_varuint32_stream(raw)
    # Expected layout: [FuncNum, ParamCount, p0, p1, ..., TimestampDelta]
    if len(vals) < 2 + nparams:
        return name, {}
    if vals[0] != func_num:
        return name, {}
    params_raw = vals[2:2 + nparams]
    field_names = PARAM_NAMES.get(name, [])
    params = {}
    for fname, v in zip(field_names, params_raw):
        params[fname] = v
    return name, params


# ============================================================================
# Parsing the remaining (non-module) columns
# ============================================================================

def task_idx_from_context(context, tcb_map):
    ctx = context.strip().strip('"')
    if ctx in TASK_NAME_TO_IDX:
        return TASK_NAME_TO_IDX[ctx]
    m = re.search(r"0x[0-9A-Fa-f]+", ctx)
    if m:
        addr = int(m.group(0), 16)
        if addr in tcb_map:
            return tcb_map[addr]
    return None


def convert(rows):
    events = []
    tcb_map = {}
    skipped = 0
    resolved_via_bytes = 0
    resolved_via_text = 0

    for row in rows:
        try:
            ts = int(row["timestampint"].strip())
        except (KeyError, ValueError):
            skipped += 1
            continue

        context = row.get("context", "")
        event   = row.get("event", "").strip()
        detail  = row.get("detail", "")
        eventdata = row.get("eventdata", "")
        try:
            func_num = int(row.get("eventint", "").strip())
        except (ValueError, AttributeError):
            func_num = None

        if event in ("Task Create", "Task Info"):
            m = re.search(r"([A-Za-z][A-Za-z0-9_]*)\s*\((0x[0-9A-Fa-f]+)\)", detail)
            if m and m.group(1) in TASK_NAME_TO_IDX:
                tcb_map[int(m.group(2), 16)] = TASK_NAME_TO_IDX[m.group(1)]
            continue

        # --- Our own module events: try plain text first, then the
        # bytes fallback ---
        mod_name = None
        params = {}
        if event in MODULE_EVENTS:
            mod_name = event
            kv = dict(re.findall(r"([A-Za-z_]\w*)=(-?(?:0x[0-9a-fA-F]+|\d+))", detail))
            if kv:
                params = {k: (int(v, 16) if v.lower().startswith("0x") else int(v))
                          for k, v in kv.items()}
                resolved_via_text += 1
            elif func_num is not None and func_num in FUNCTION_NUM_TO_NAME:
                mod_name, params = resolve_via_bytes(func_num, eventdata)
                if params:
                    resolved_via_bytes += 1
        elif func_num is not None and func_num in FUNCTION_NUM_TO_NAME:
            mod_name, params = resolve_via_bytes(func_num, eventdata)
            if params:
                resolved_via_bytes += 1

        if mod_name is not None:
            if mod_name == "TaskMap":
                idx, addr = params.get("Idx"), params.get("Addr")
                if idx is not None and addr is not None:
                    tcb_map[addr] = idx
                    if addr >= 0x20000000:
                        tcb_map[addr - 0x20000000] = idx
                continue
            handler = MODULE_EVENTS.get(mod_name)
            if handler is not None and params:
                for stream, val in handler(params):
                    events.append((ts, stream, val))
            continue

        # --- Base SystemView events ---
        idx = task_idx_from_context(context, tcb_map)

        if event == "Task Ready":
            m = re.match(r"([A-Za-z][A-Za-z0-9_]*)", detail.strip().strip('"'))
            if m and m.group(1) in TASK_NAME_TO_IDX:
                events.append((ts, "ready", TASK_NAME_TO_IDX[m.group(1)]))
            elif idx is not None:
                events.append((ts, "ready", idx))
            else:
                skipped += 1
            continue

        if event == "Task Run":
            if idx is not None:
                events.append((ts, "exec", idx))
            else:
                skipped += 1
            continue

        if event == "Task Stop":
            continue

        if event == "Task Block":
            det = detail.strip().strip('"')
            m = re.match(r"([A-Za-z][A-Za-z0-9_]*)", det)
            if m and m.group(1) in TASK_NAME_TO_IDX:
                events.append((ts, "blocked", TASK_NAME_TO_IDX[m.group(1)]))
            elif idx is not None:
                events.append((ts, "blocked", idx))
            else:
                skipped += 1
            continue

        if event == "ISR Enter":
            ctx_clean = context.strip().strip('"')
            ISR_NAME_TO_NUM = {"SysTick": 15}
            num_m = re.search(r"#?(\d+)", ctx_clean)
            if num_m:
                isr_num = int(num_m.group(1))
            elif ctx_clean in ISR_NAME_TO_NUM:
                isr_num = ISR_NAME_TO_NUM[ctx_clean]
            else:
                isr_num = -1
            events.append((ts, "isr_enter", isr_num))
            continue

        if event == "ISR Exit":
            events.append((ts, "isr_exit", None))
            continue

        skipped += 1

    events.sort(key=lambda e: e[0])
    last = {}
    fixed = []
    for ts, stream, val in events:
        if stream in last and ts <= last[stream]:
            ts = last[stream] + 1
        last[stream] = ts
        fixed.append((ts, stream, val))
    fixed.sort(key=lambda e: e[0])
    return fixed, skipped, resolved_via_text, resolved_via_bytes


def streams_from_specs(paths):
    wanted = set()
    in_re = re.compile(r"^\s*in\s+([A-Za-z_][A-Za-z0-9_]*)\s*:")
    for p in paths:
        with open(p, encoding="utf-8") as f:
            for line in f:
                m = in_re.match(line)
                if m:
                    wanted.add(m.group(1))
    return wanted


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("export", help="SystemView CSV export")
    ap.add_argument("-o", "--output", default="trace.input")
    ap.add_argument("--spec", action="append", default=[])
    args = ap.parse_args()

    with open(args.export, encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        events, skipped, via_text, via_bytes = convert(list(reader))

    wanted = streams_from_specs(args.spec) if args.spec else None
    n = 0
    with open(args.output, "w", encoding="utf-8") as out:
        for ts, stream, val in events:
            if wanted is not None and stream not in wanted:
                continue
            v = "()" if val is None else str(val)
            out.write(f"{ts}: {stream} = {v}\n")
            n += 1

    counts = {}
    for _, s, _ in events:
        counts[s] = counts.get(s, 0) + 1
    print(f"{n} events -> {args.output}  ({skipped} lines skipped)")
    print(f"Module events resolved: {via_text} via plain text, {via_bytes} via bytes fallback")
    for s in sorted(counts):
        mark = "" if (wanted is None or s in wanted) else "  [filtered out]"
        print(f"  {s:28s} {counts[s]:6d}{mark}")


if __name__ == "__main__":
    main()
