# TeSSLa Verification — DOS-RTOS

This directory contains the runtime-verification specifications for the
DOS-RTOS project, the SystemView-to-TeSSLa converters, and the results of
checking each specification against real recordings from the target
(STM32L475VG).

## Contents

| File | Purpose |
|---|---|
| `mutex.tessla` | Mutex rules — **verification** (used with the interpreter) |
| `semaphore.tessla` | Binary-semaphore rules — verification |
| `delay.tessla` | Delay rules — verification |
| `queue.tessla` | Message-queue rules — verification |
| `scheduler_tasks.tessla` | Scheduler and task-state rules — verification |
| `sensor.tessla` | Project-specific 100ms transmission cadence — verification |
| `*_playground.tessla` | Same rule logic, plus `out`/`@Vis*` annotations for `play.tessla.io` — **visualization only, not used to determine pass/fail** |
| `sysview_to_tessla_bytes.py` | Converts a SystemView CSV export to a TeSSLa trace by decoding the raw VarUint32 payload bytes — the tool actually used to produce every result below |
| `sysview_to_tessla.py` | Earlier converter relying on SystemView's text labels for our own module's events; superseded by `sysview_to_tessla_bytes.py` (see "Why two converters" below) |
| `trace_excerpt.py` | Cuts a time window out of a `.input` trace, optionally filtered by stream — used to build small excerpts for the Playground |

## How to run a check

```bash
python3 sysview_to_tessla_bytes.py <export>.csv -o trace.input
java -jar tessla.jar interpreter <spec>.tessla trace.input
```

A **passed** check means every `violation_*` stream in the output stays
empty. Lines starting with `info_*` are not failures — they report
observations (e.g. rejected foreign unlocks, sensor error frames) as
evidence that the corresponding behavior actually occurred and was
handled correctly.

Two separate recordings are needed, because `scheduler_tasks.tessla`
requires ISR events that the other five specs deliberately leave out to
keep the trace small and overflow-safe:

| Recording | `os_trace_config.h` setting | Used for |
|---|---|---|
| A | `OS_RUN_INTEGRATION_TESTS = 1`, `OS_TRACE_ISR = 0` | mutex, semaphore, delay, queue |
| B | `OS_TRACE_ISR = 1` | scheduler_tasks |
| (not yet recorded) | `OS_RUN_INTEGRATION_TESTS = 0` (normal operation, HC-SR04 connected) | sensor |

## Requirement coverage and status

The following is the complete project requirement list under "Generelle
Projektspezifische Anforderungen an die Verifikation (mit TeSSLa!)",
mapped to the rule that checks it and its current status.

### Scheduler

| Requirement | Rule | Status |
|---|---|---|
| Higher-priority tasks always run before lower-priority ones | `violation_priority_order` | ✅ Passed |
| Tasks of equal priority are scheduled round-robin | `violation_round_robin` | ✅ Passed |
| A task must not exceed its time slice | `violation_timeslice` | ✅ Passed |
| The idle task only runs when no other task is ready | `violation_idle_only_when_idle` | ✅ Passed |
| After an ISR, the interrupted task resumes | `violation_isr_scheduling` | ✅ Passed |
| Only valid state transitions occur | `violation_invalid_transition` | ✅ Passed |

*(`scheduler_tasks.tessla`, recording B, `OS_TRACE_ISR = 1`)*

### Tasks

| Requirement | Rule | Status |
|---|---|---|
| A task is in exactly one state at a time | true by construction of `state(i)` | ✅ (see note below) |
| A task is never READY and BLOCKED simultaneously | `violation_invalid_transition` | ✅ Passed |
| BLOCKED tasks are never executed | `violation_exec_while_blocked` | ✅ Passed |
| A task only undergoes valid state transitions | `violation_invalid_transition` | ✅ Passed |

Note on "exactly one state at a time": `state(i)` is a single-valued
stream by definition — a task cannot simultaneously *be* two states in
this encoding. What is actually checked is that the *sequence* of
transitions the trace implies is internally consistent (no jump that
would only make sense if two states had overlapped); that is exactly
`violation_invalid_transition`.

### Mutexes

| Requirement | Rule | Status |
|---|---|---|
| A mutex has at most one owner | `violation_*_double_lock` | ✅ Passed |
| Only the owner may release a mutex | `violation_*_unlock_not_owner` | ✅ Passed |
| A locked (non-recursive) mutex cannot be re-acquired | `violation_*_double_lock` | ✅ Passed |
| Blocking acquire moves the task to BLOCKED | `violation_*_block_state` | ✅ Passed |
| Releasing wakes a waiting task | `violation_*_wake_after_unlock` | ✅ Passed |
| Waiting tasks are woken in priority order | `violation_*_waiter_priority` | ✅ Passed |

*(`mutex.tessla`, recording A, both `g_uartMutex` and `g_configMutex`)*

### Binary semaphores

| Requirement | Rule | Status |
|---|---|---|
| The value is only ever 0 or 1 | `violation_*_value_range` | ✅ Passed |
| An already-set semaphore cannot be given again | `violation_*_double_give` | ✅ Passed |
| An empty semaphore cannot be taken successfully | `violation_*_take_from_empty` | ✅ Passed |
| Blocking acquire moves the task to BLOCKED | `violation_*_block_state` | ✅ Passed |
| A timeout acquire ends once its time has elapsed | `violation_*_timeout_early`, `violation_*_timeout_must_end` | ✅ Passed |
| Releasing wakes a waiting task | `violation_*_wake_after_give` | ✅ Passed |
| Waiting tasks are woken in priority order | `violation_*_waiter_priority` | ✅ Passed |

*(`semaphore.tessla`, recording A, both `g_echoDoneSemaphore` and `g_uartRxSemaphore`)*

### Delays

| Requirement | Rule | Status |
|---|---|---|
| Non-blocking delays move the task to BLOCKED | `violation_delay_block_state` | ✅ Passed |
| Blocking delays cause no state change | `violation_busy_state_change` | ✅ Passed |
| A task is not woken before its delay elapses | `violation_delay_early_wake` | ✅ Passed |
| After the delay elapses, the task becomes READY | `violation_delay_ready_after` | ✅ Passed |

*(`delay.tessla`, recording A)*

### Message queues

| Requirement | Rule | Status |
|---|---|---|
| Fill level is never negative or above capacity | `violation_q*_count_range` | ✅ Passed |
| Messages are received in FIFO order | — | ⚠️ Not checked by TeSSLa (see below) — demonstrated functionally by integration test T18 |
| Sent messages arrive unmodified | — | ⚠️ Same as above — T18 |
| Reading from an empty queue does not succeed | `violation_q*_recv_from_empty` | ✅ Passed |
| Writing to a full queue does not succeed | `violation_q*_send_to_full` | ✅ Passed |
| Blocking send/receive move the task to BLOCKED | `violation_q*_block_state` | ✅ Passed |
| Timeout operations end once their time has elapsed | `violation_q*_timeout_must_end` | ✅ Passed |
| Releasing a slot wakes a waiting sender | `violation_q*_wake_sender` | ✅ Passed |
| Arrival of a message wakes a waiting receiver | `violation_q*_wake_receiver` | ✅ Passed |

*(`queue.tessla`, recording A, both `g_sensorQueue` and `g_processedQueue`)*

**FIFO order and message integrity are disabled in `queue.tessla`.** The
rule was implemented as a list-based reconstruction of pending
checksums (`fifo`/`fifoBad`, still present but commented out in the
file) and is correct in principle, but the TeSSLa interpreter's
`List.append`/`List.tail` on every event scales poorly: on a recording
with roughly 80+ queue operations it reproducibly throws
`OutOfMemoryError`, even with an enlarged Java heap (`-Xmx8g`). Instead,
both properties are demonstrated **functionally** by integration test
T18 (`tests.c`): 16 messages carrying a known checksum pattern are sent
through a queue smaller than the message count, so both sender and
receiver block partway through — receipt in order with the correct
checksum is verified in the firmware itself and reported over UART
(`[OK] T18 Queue: FIFO order and data integrity`).

### Project-specific: sensor transmission cadence

| Requirement | Rule | Status |
|---|---|---|
| Distance readings are transmitted every 100ms (±15ms) | `violation_tx_period` | ⚪ Written, not yet run against a recording |

`sensor.tessla` needs a recording from normal operation
(`OS_RUN_INTEGRATION_TESTS = 0`) with the HC-SR04 connected; none of the
sessions so far captured one after the tick-length correction (see
below). The rule logic itself follows the same pattern as the other
five specs and compiles/type-checks; it has simply not yet been
exercised against real data.

## Notes that apply across all specs

**Tick length.** All specs use `TICK = 80000` (ns, i.e. 80µs) —
measured empirically from a real recording (gap between consecutive
`Task Stop` events of the idle task, which coincide exactly with each
SysTick), not the 1ms that would be the default assumption for a
SysTick-based scheduler. A semaphore-timeout test case
(`OS_Semaphore_TakeTimeout(&sem, 20)`) fired after 1.6ms rather than the
expected 20ms; `20 × 80000ns = 1.6ms` matches exactly. The scheduler
counts down correctly in its own ticks — the wrong assumption was only
in the specs. Whether an 80µs tick is a deliberate design choice (finer
scheduling granularity) or a SysTick reload-value misconfiguration on
the target has not been determined.

**Warm-up window.** All specs ignore the first 250ms (`WARMUP`, a fixed
nanosecond value, deliberately *not* expressed as a multiple of `TICK`):
recording starts mid-flight, and the reconstructed state needs some real
time to converge with reality regardless of how long a tick is.

**Per-task ticks streams.** Wait durations (`Ticks`) for delays,
semaphore blocks, and queue blocks are delivered as one stream per task
(`delay_ticks_0` … `_3`, `sem_echo_block_ticks_0` … `_3`, etc.) rather
than a single shared stream. A shared stream requires filtering by task
index inside the spec (`filter(ticks, task == i)`), which broke down
once multiple tasks had overlapping blocking calls — combined with the
converter nudging identical timestamps within one stream apart by 1ns,
the task and its duration no longer landed on the exact same timestamp,
and duration values ended up misattributed to the wrong task. This
produced dozens of false violations in `delay.tessla` and
`semaphore.tessla` before being tracked down. Splitting the stream per
task removes the ambiguity structurally.

## Why two converters

`sysview_to_tessla.py` resolves our own module's events (`MtxLockOk`
etc.) via the plain-text names SystemView shows once it has received the
module description. In several recordings this resolution repeatedly
failed — events kept showing up as `Function #NNN` with an empty detail
field even though the full module description had been transmitted; the
root cause was not conclusively identified.

`sysview_to_tessla_bytes.py` sidesteps the problem entirely: it decodes
every module event directly from the raw `eventdata` column. SystemView
encodes parameters there as VarUint32 (LEB128-style) values, laid out as

```
[0]   function number   (EventOffset + enum index)
[1]   parameter count    (2 or 3)
[2..] the parameters      (e.g. Mtx, Task)
[last] a timestamp delta  (ignored)
```

This layout was reverse-engineered and cross-checked against several
known-good `Mtx=… Task=…` event instances before being trusted. Every
result in this document was produced with `sysview_to_tessla_bytes.py`;
it does not depend on SystemView resolving any text label.

## Bugs found and fixed along the way

Three converter bugs would have produced **false** violations if left
unfixed, and were only caught by comparing TeSSLa output against the raw
CSV rows:

1. **ISR number as text.** SystemView writes `"SysTick"` into the
   context column, not the exception number 15. The converter fell back
   to 0, so `inSysTick` in `scheduler_tasks.tessla` was never true and
   every single `exec` event looked like a violation.
2. **`Task Ready` target is in the detail field.** The context column
   holds the *waking* context; the woken task is in the detail text
   (`"Sensor, runs after 771.063 us"`).
3. **`Task Block` — the same pattern.** Context showed `"Idle"` while the
   detail held `"UartShell, Reason=3"`; the converter attributed the
   block to the wrong task.

The integration tests (`tests.c`) themselves also went through three
rounds of fixes, all of them concurrency bugs in the *test* code, not
the kernel:

1. A live-lock from too short a poll interval in the high-priority test
   task, starving the very task it was waiting on.
2. A deadlock from two test tasks setting/waiting-on synchronization
   phases in mismatched order.
3. A stale result being evaluated after the tests were changed to loop
   indefinitely, which could have reported `[OK]` for a timed-out
   handshake.

See the main project `README.md` for the full account, including the
hardware/toolchain issues encountered before any of this trace work
could even begin.

## Playground visualization

The `*_playground.tessla` files are for visual inspection on
[play.tessla.io](https://play.tessla.io) and are **not** part of the
formal verification — passing or failing is determined exclusively by
running the corresponding non-`_playground` spec through the CLI
interpreter as shown above.

**Why a separate file at all:** the Playground only plots streams that
are declared `out`. A spec written purely for verification only exposes
its `violation_*`/`info_*` streams — on a passing run (empty
`violation_*`) the Playground then shows nothing, because it has nothing
declared to draw. The playground variants additionally declare the raw
event streams (`mtx_uart_ok`, `sem_echo_block`, `exec`, reconstructed
owners, …) as `out`, and annotate them with `@VisEvents` (discrete
cross-marks — locks, blocks, timeouts) or `@VisSignal` (filled step
plot — for streams that represent an ongoing state, like "which task is
currently running" or "who currently owns this mutex").

**Trace-editor format.** The Playground's trace editor (top left) wants
exactly the plain `<timestamp_ns>: <stream> = <value>` lines that
`sysview_to_tessla_bytes.py` already produces — no header, no `stream …`
declarations, no `---` separator. (That combined syntax belongs to the
**output** tab, which is a different part of the tool, and pasting it
into the trace editor produces a parser error.)

**Walkthrough:**

1. Paste a `*.tessla` file (not `*_playground.tessla` — start with a
   plain spec to confirm the format) plus a small hand-written trace
   into the Specification/Trace editors, click Run, and check the
   "TeSSLa Visualization" tab renders something. This isolates a
   pasting mistake from an actual problem before working with the full
   trace.
2. A full recording easily exceeds 20,000 lines, which is unwieldy (and
   was, in practice, the actual cause of an early parser error — not the
   trace format itself). Cut a smaller window instead:

   ```bash
   python3 trace_excerpt.py trace.input --stats
   python3 trace_excerpt.py trace.input -v <timestamp_ns> -d 15 -o excerpt.input
   ```

   A window of a few hundred events around a specific occurrence (e.g. a
   recorded `mtx_cfg_timeout`) is both large enough to show the
   surrounding context and small enough to paste comfortably.
3. Paste `excerpt.input` into the Trace editor and the matching
   `*_playground.tessla` into the Specification editor, Run, then switch
   to "TeSSLa Visualization".

Verified working end-to-end for `mutex_playground.tessla` against an
excerpt around a recorded `mtx_cfg_timeout` — reconstructed mutex
ownership and task execution rendered correctly as filled step plots
alongside the discrete lock/block/unlock events.
