# DOS-RTOS — Real-Time Systems Final Project

A custom pre-emptive RTOS for the **STM32L475VG** (B-L475E-IOT01A) with HC-SR04 distance measurement, an interactive UART shell, and runtime verification via SEGGER SystemView → TeSSLa.

This document first describes **how to get the project running**, then the **architecture**, the **deliberate decisions** (including deviations from the original plan), and the **problems and their fixes** encountered during integration. The last section is deliberately thorough: a large part of the project work was not writing the kernel, but building a working trace and verification chain.

---

## 0. Build Instructions

### 0.1 Prerequisites

- **STM32CubeIDE** (tested with 1.19.0)
- Board: **B-L475E-IOT01A** (STM32L475VG)
- HC-SR04 ultrasonic sensor
- A terminal program for UART (PuTTY on Windows, `screen`/Terminal on macOS/Linux) and **SEGGER SystemView** (tested with V4.10b) for trace recording
- For verification, additionally: **Java** (for the TeSSLa interpreter) and **Python 3** (for the converter scripts in `tessla/`)

### 0.2 Path A — Build an existing CubeIDE project

If a `.ioc`/CubeIDE project with all files from this repository already exists:

1. Import the project into CubeIDE (**File → Import → Existing Projects into Workspace**)
2. **Project → Clean**, then **Build**
3. Check the compiler line in the build console: `-mfloat-abi=soft`, **no** `-mfpu=…` — if it shows `hard`, see 0.4
4. Flash (Run or Debug)
5. **Power-cycle the board** (unplug USB, wait ~10s, plug back in — more reliable than the reset button alone, see 3.2)
6. Open a UART terminal (115200 8N1, ST-LINK Virtual COM Port), type `help` — on success the shell replies with the command overview

### 0.3 Path B — Set up the project from scratch

If no finished `.ioc` project exists, here is the full path through
CubeMX:

1. **File → New → STM32 Project**, search the board selector for
   "B-L475E-IOT01A", select it, **Next → Finish**. When asked about the
   default peripheral initialization: **Yes**.
2. **Pinout & Configuration:**
   - Connectivity → **USART1** → Mode: Asynchronous; in the NVIC tab of
     the same page, enable "USART1 global interrupt"
   - In the Pinout view: **PA4** → GPIO_Output (TRIG), **PB0** →
     GPIO_EXTI0 (ECHO)
   - System Core → **NVIC**, "Code generation" tab: **enable** "EXTI
     line0 interrupt"; for "Pendable request for system service"
     (PendSV), **uncheck** the box under "Generate IRQ handler"
     (otherwise a linker error "multiple definition" against
     `pendsv.s`)
3. **Clock Configuration:** enter **80 MHz** for HCLK, press Enter — MSI
   as the source (the board has no HSE crystal), CubeMX computes PLL,
   voltage scale, and flash latency automatically
4. **Project Manager → Project:** Toolchain/IDE = STM32CubeIDE
5. **GENERATE CODE**, when asked about switching perspective: **Yes**
6. **Change the FPU setting** (not possible directly in the `.ioc`, only
   now): right-click the project → **Properties → C/C++ Build →
   Settings → Tool Settings → MCU Settings** →
   - **Floating-point unit: None**
   - **Floating-point ABI: Software implementation (-mfloat-abi=soft)**

   Both fields are **independent** dropdowns in CubeIDE — "FPU: None"
   alone is not enough, the ABI field must be set to `soft` separately
   (see 3.1 for the background).
7. **Add the SEGGER files:** create a `Core/Segger` folder, copy in
   `SEGGER_RTT.c/h`, `SEGGER_RTT_ASM_ARMv7M.S`, `SEGGER_SYSVIEW.c/h`,
   `SEGGER_SYSVIEW_Conf.h`, and the already-patched
   `SEGGER_SYSVIEW_Config_NoOS.c` included in this repository. Add the
   include path: **MCU/MPU GCC Compiler → Include paths** → green plus
   → **Workspace…** button (don't type it freehand) → select
   `Core/Segger`.
8. **Copy in the project's own source files:** all `.c`/`.h` files from
   this repository into `Core/Src`/`Core/Inc`, `pendsv.s` into
   `Core/Src` as well.
9. **main.c, stm32l4xx_it.c, stm32l4xx_hal_msp.c:** do **not** overwrite
   the generated CubeMX files here — instead transfer the USER CODE
   blocks (includes, task init, DBGMCU lines, EXTI/USART1 handlers)
   into the freshly generated files at the matching
   `/* USER CODE BEGIN … END */` markers. This repository's `main.c` is
   already complete and can replace the generated one directly;
   likewise for `stm32l4xx_it.c`. In the generated
   `stm32l4xx_hal_msp.c`, the USER block of `USART1_MspInit` needs the
   NVIC lines added (CubeMX generates GPIO and clock setup, but not the
   NVIC activation):
   ```c
   HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
   HAL_NVIC_EnableIRQ(USART1_IRQn);
   ```
10. **`tcb.h`**: set `TCB_TASK_STACK_SIZE` to **256** — the only
    permitted change to this file.
11. Continue with step 3 from Path A (Clean, Build, check the compiler
    line, flash, power-cycle, test UART).

### 0.4 Most common pitfall: FPU still set to `hard`

If the compiler line keeps showing `-mfloat-abi=hard` even though
"Floating-point unit" is already set to `None`: **Floating-point unit**
and **Floating-point ABI** are two separate dropdown fields in CubeIDE
that are not reliably linked. Both must be changed individually (see
step 6 above). Without this fix, the PendSV handler does not save FPU
registers on a context switch — a rare, hard-to-reproduce crash is the
result.

### 0.5 Trace recording and verification

For pure firmware functionality (distance measurement, shell), 0.2/0.3
is sufficient. For the TeSSLa verification chain (SystemView recording
→ converter → TeSSLa interpreter), see **Section 4** further below and
`tessla/README.md` (or `tessla/README_de.md`) for the full walkthrough
covering all six specifications and their status.

Short form:

```bash
# Set os_trace_config.h accordingly first (see Section 4)
python3 tessla/sysview_to_tessla_bytes.py <export>.csv -o trace.input
java -jar tessla.jar interpreter tessla/<spec>.tessla trace.input
```

---

## 1. System Architecture

### 1.1 Taskset

| Idx | Task        | Prio | Role |
|-----|-------------|------|---------|
| 0   | `SensorTask`    | 3 | Triggers the HC-SR04 on a 100ms cadence, waits for the echo, sends raw readings |
| 1   | `ProcTask`      | 1 | Applies calibration, forwards processed readings |
| 2   | `UartShellTask` | 1 | UART output and command processing |
| 3   | `IdleTask`      | 0 | Idle, **must be the last entry** (scheduler convention) |

**Why these priorities?** The sensor needs the highest priority because
its 100ms cadence is time-critical — delayed trigger pulses directly
corrupt the measurement. `Proc` and `UartShell` deliberately form a
**pair of equal priority**: only that way does genuine round-robin
behavior arise during normal operation, which is what lets the
corresponding TeSSLa rule be checked at all. Idle sits at priority 0 at
the last index by convention; the scheduler uses
`tasks[NUM_TASKS - 1]` as a fallback.

### 1.2 Data Flow

```
  HC-SR04 ──(EXTI, both edges)──> ISR measures pulse duration via DWT->CYCCNT
                                          │ releases g_echoDoneSemaphore
                                          ▼
  SensorTask ──> g_sensorQueue ──> ProcTask ──> g_processedQueue ──> UartShellTask ──> UART
                                       │                                   │
                                  g_configMutex                       g_uartMutex
                                (calibration values)                (output protection)
```

The shell also writes calibration values under `g_configMutex` — that
is the only point where genuine mutex contention can arise during
normal operation.

### 1.3 Scheduling

Priority-based pre-emptive round-robin. The **SysTick** (1 tick) is the
only place that makes scheduling decisions; the actual context switch
runs in the subsequent **PendSV** (lowest priority, assembly in
`pendsv.s`).

**Decision: only SysTick schedules.** Peripheral ISRs (EXTI for the
echo, USART1 for the shell) exclusively release semaphores and return
to the interrupted task. This satisfies the requirement "after an ISR,
the interrupted task resumes" structurally rather than merely testing
for it — and is at the same time verified in `scheduler_tasks.tessla`
via the observable consequence (exec events occur exclusively inside a
SysTick window).

**Interrupt priorities:** SysTick 0, EXTI0 5, USART1 6, PendSV 15.

### 1.4 Blocking Mechanics of the Kernel Objects

All three object types (mutex, binary semaphore, queue) each offer
**NonBlocking / Blocking / Timeout**. A single, uniform pattern
underlies all of them:

Every object carries a **wait mask** (bit *i* = `tasks[i]` is waiting;
the queue has separate masks for senders and receivers). The flow
inside the kernel:

1. Enter a critical section (save PRIMASK)
2. Clear the task's own wait bit — **only on retry**, not on the first
   pass
3. Attempt the resource → `OS_OK` / `OS_WOULD_BLOCK` / `OS_TIMEOUT`
4. Otherwise: set the wait bit, block the task, spin until woken

On release, **all** waiters are woken (the mask is cleared entirely);
the highest-priority one then wins the retry via the scheduler, the
losers block again with their **remaining time**.

**Why is the wait bit only cleared on retry?** A NonBlocking call from
an ISR would otherwise destroy the wait bit of the task that was just
interrupted while blocked — that task would never be woken again. This
bug was present in the original design and only surfaced during
review.

**Why is the delay counter not cleared on wake-up?** So that a task
that loses the retry race continues waiting with the correct remaining
time instead of immediately reporting a false `OS_TIMEOUT`.

### 1.5 Hardware Mapping

| Signal | Pin | Note |
|---|---|---|
| HC-SR04 TRIG | **PA4** (Arduino D7) | Push-pull, 3.3V is sufficient for the sensor |
| HC-SR04 ECHO | **PB0** (Arduino D3, EXTI0) | via a 1kΩ/2kΩ voltage divider |
| UART TX/RX | **PB6/PB7** (USART1) | hardwired to the ST-LINK Virtual COM Port |

System clock **80MHz** from MSI (4MHz) + PLL — the board has no HSE
crystal on the main clock.

**FPU disabled** (Floating-point unit `None`, ABI `soft`): the PendSV
handler only saves R4–R11, no FPU registers. With a hard-float ABI, the
compiler could keep FPU registers live across function boundaries —
these would be destroyed on a context switch.

---

## 2. Deliberate Decisions and Deviations

### 2.1 Idle Task: Deliberately Empty

`IdleTask()` contains **neither `SEGGER_SYSVIEW_OnIdle()` nor
`__WFI()`** — just an empty infinite loop. This deviates from the
requirement "instrument the IDLE task", for a demonstrated technical
reason:

- **With `__WFI()`**, the core sleeps between interrupts. In this state,
  SystemView no longer reliably finds the RTT control block ("Could not
  find RTT Control Block", timeout after 20s) — SWD access to a
  sleeping STM32L4 is not guaranteed without additional debug
  enables. We set the intended
  `HAL_DBGMCU_EnableDBGSleepMode()` / `...StopMode()` / `...StandbyMode()`
  calls; the problem still occurred reproducibly.
- **Without `__WFI()`, but with `OnIdle()`**, the idle loop spins freely
  at 80MHz and fires idle events as fast as the processor can. The RTT
  buffer then overflows within milliseconds ("SystemView overflow
  events recorded"), and a trace with data loss is worthless for
  verification.

**Consequence for verification:** idle time is still visible in
SystemView — as gaps between task segments and via idle's own lane on
the timeline. What is missing is explicit `OnIdle` events. A stable,
gap-free trace mattered more to us than this one event type, because
without it *none* of the TeSSLa rules could have been checked with
confidence.

### 2.2 Shortened Module Description

`os_trace.c` currently registers the SystemView module `DOSRTOS` with a
**shortened description** (only the six mutex events named). The
full variant with all 30 events sits directly below it as an `#if 0`
block.

Reason: the full string is ~900 characters long. To transmit it in
full at all, `SEGGER_SYSVIEW_MAX_STRING_LEN` and `BUFFER_SIZE_UP` had
to be raised significantly — which in turn reproducibly triggered an
RTT overflow right after boot. Since our converter decodes events
**in binary** anyway (see 3.4) and does not need the plain-text names,
the short variant is the more stable choice.

### 2.3 Trace Groups Switchable via `#define`

`os_trace_config.h` contains group switches (`OS_TRACE_SCHEDULER`,
`OS_TRACE_ISR`, `OS_TRACE_MUTEX`, `OS_TRACE_SEMAPHORE`,
`OS_TRACE_QUEUE`, `OS_TRACE_DELAY`, `OS_TRACE_APP`), a master switch
(`OS_TRACE_ENABLED`), and `OS_TRACE_TRY_EVENTS`.

Disabled groups cost **nothing** at runtime: the macros become empty
statements, the compiler removes both the call *and* the parameter
computation. This satisfies the requirement "events activatable via
#define" and is at the same time the practical tool against RTT
overflows — for a targeted verification run, everything the respective
spec does not need can be switched off.

`OS_TRACE_TRY_EVENTS` defaults to **0**: the "Try" events roughly
double the event count of their group but are evaluated by none of the
six specs.

### 2.4 Integration Tests Instead of Random Contention

During normal operation, blocking, timeout, and contention cases only
arise by chance — in practice, never: a trace of normal operation
contained exclusively successful lock/unlock pairs, not a single
`MtxLockBlock`. This means rules such as "priority ordering of waiting
tasks" cannot be *checked*, only found not-violated for lack of
opportunity.

`tests.c` therefore provides an **alternative taskset**
(`OS_RUN_INTEGRATION_TESTS = 1` in `os_trace_config.h`) that
deliberately provokes these cases: 18 test cases, results reported as
`[OK]`/`[FAIL]` over UART.

The test objects deliberately use **the same trace IDs** as the
application objects. This means `mutex.tessla`, `semaphore.tessla`, and
`queue.tessla` apply unchanged to the test-run trace too — and *that*
is the actual proof that the rules hold under contention.

The test sequence runs in an **infinite loop**. Originally it ran once
and the tasks then slept; that turned recording into a game of chance,
since the narrow window right after boot had to be hit. With the
repetition, SystemView can be connected at any time.

---

## 3. Problems and Fixes

This section documents the detours — partly because they are needed to
understand the final state, partly because they show where the actual
pitfalls were.

### 3.1 Toolchain and Project Setup

**FPU still on hard-float despite `FPU = None`.** In CubeIDE,
"Floating-point unit" and "Floating-point ABI" are **two separate
dropdowns** that are not reliably linked. The compiler invocation kept
showing `-mfloat-abi=hard` even though the FPU was already set to
`None`. Check: read the actual compiler line in the build console, not
the dialog.

**Include path to the SEGGER folder.** Two different errors in
succession: first a `${workspace_loc:...}` expression that failed to
resolve on Windows (fix: relative path `../Core/Segger`), then the same
path accidentally entered into the **"Include files (-include)"**
field instead of **"Include paths (-I)"**. Both fields sit directly on
top of each other in the dialog.

**Duplicate symbols after CubeMX regeneration.** `HAL_UART_MspInit`
(our version in `main.c` vs. the generated one in
`stm32l4xx_hal_msp.c`) and `PendSV_Handler` (generated empty version
vs. our `pendsv.s`). Permanent fix: in the `.ioc`, under *System Core →
NVIC → Code generation*, uncheck "Generate IRQ handler" for **Pendable
request**, and maintain `HAL_UART_MspInit` exclusively in the generated
`stm32l4xx_hal_msp.c` — adding, however, the NVIC lines for USART1 in
the USER CODE block, which CubeMX does not generate itself.

**`__HAL_RCC_DBGMCU_CLK_ENABLE()` does not exist on the L4.** Unlike
peripheral clocks, DBGMCU has no switchable APB clock.

**Support for Apple Silicon** was not properly provided, even though an
application for ARM-based Macs actually exists. Instead, we used the Intel
version via Rosetta because it ran more consistently. However, we had to
sacrifice some calculation speed for the sake of greater stability.

### 3.2 Hardware and Bring-up

**Boot order: SysTick before the scheduler.** `HAL_Init()` starts
SysTick immediately, but `Scheduler_vInit()` only runs considerably
later. In between, every tick called `Scheduler_pGetNextTask()` with
`g_pCurrentTask == NULL` → a HardFault before the first task even ran.
Fix: boot guards in `Scheduler_vCountdown()` and
`Scheduler_pGetNextTask()`, plus atomically setting both pointers in
`Scheduler_vInit()`.

**PB4/PB5 are occupied on the IOT01A.** The originally planned pins for
TRIG/ECHO are, per UM2153, hardwired to the on-board sub-GHz radio
module (PB5 = SPSGRF-SPI3_CSN). Moved to **PA4/PB0**, both available on
the Arduino header. Since PB0 sits on EXTI line 0, the handler is
`EXTI0_IRQHandler` rather than `EXTI9_5_IRQHandler`.

**The reset button is not always enough.** It only resets the STM32 —
the ST-LINK/J-Link debug chip keeps its internal connection state.
After aborted SWD sessions, only this helped: **unplug the USB cable,
wait ~10s, plug it back in.** That resets both chips. This insight
resolved several hours of what looked like firmware debugging.


At the beginning, we were handed **two different versions of STM boards.** 
We started our project using an STM32L475 and a CubeF4. To support both boards, 
we implemented a board_config.h file. However, several issues arose during
development due to the hardware differences. Fortunately, another group of 
students was kind enough to give us one of their STM32L475 boards, allowing us 
to finish the project with two identical boards in the end.


### 3.3 SystemView

**Task names were missing** (`Task 0x11EC` instead of `Idle`).
SystemView only shows names if it caught the `SendTaskInfo` events. Two
fixes: start the recording **before** the board reset, or wire up the
`pfSendTaskList` callback in `SEGGER_SYSVIEW_Config_NoOS.c`
(`OS_Trace_vSendTaskList` as an OS API entry) so SystemView actively
requests the list on every connection.

**Module description got truncated.** The description string broke off
mid-way through `MtxLockTimeout Mtx=%u` —
`SEGGER_SYSVIEW_MAX_STRING_LEN` was too small (default well under the
~900 characters needed). Raising it fixed the truncation but produced
an RTT overflow. See 2.2 for the consequence we chose.

**`BUFFER_SIZE_UP` belongs in `SEGGER_RTT_Conf.h`**, not in
`SEGGER_SYSVIEW_Conf.h` — the two config files are read by different
code.

**Function names were never resolved.** Even with a fully transmitted
module description, SystemView kept showing our own events as
`Function #513` instead of `MtxLockOk`, with an empty detail field. The
root cause was not conclusively identified; the events themselves
arrived with the correct number and correct parameters. Fix: see 3.4.

### 3.4 Trace Converter (`sysview_to_tessla.py` / `sysview_to_tessla_bytes.py`)

The converter translates the SystemView CSV export into the TeSSLa
input format. Three bugs had to be fixed based on real recordings —
each of them would have produced false rule violations:

1. **ISR number as plain text.** SystemView writes `"SysTick"` into the
   context column, not the exception number 15. The converter fell
   back to `0`, so `inSysTick` in the spec was never true and *every*
   exec event looked like a violation.
2. **`Task Ready`: the target is in the detail field.** The context
   column holds the *waking* context; the woken task is in the detail
   (`"Sensor, runs after 771.063 us"`).
3. **`Task Block`: the same pattern.** Context showed `"Idle"` while the
   detail held `"UartShell, Reason=3"` — the converter attributed the
   block to the wrong task.

**Binary fallback decoding.** SystemView failed to reliably resolve the
plain-text names of our own module's events (`MtxLockOk` etc.) and
their detail parameters across several recording sessions — even with
a fully transmitted module description, they kept showing up as
`Function #NNN` with an empty detail field; we could not conclusively
determine the cause. `sysview_to_tessla_bytes.py` sidesteps this: it
decodes every event directly from the `eventdata` column. SystemView
encodes **VarUint32** values there (LEB128-style), laid out as:

```
[0] function number   (EventOffset + enum index)
[1] parameter count   (2 or 3)
[2..] the parameters  (e.g. Mtx, Task)
[last] a timestamp delta (ignored)
```

Calibrated and verified against real recordings. This makes the
verification chain **independent** of whether SystemView resolves the
names.

**Per-task ticks streams.** Originally, the wait duration (`Ticks`) for
delays, semaphore blocks, and queue blocks ran over a *shared* stream
(e.g. `delay_ticks`), from which the spec had to filter out the right
value via `filter(ticks, task == i)`. Once several tasks were waiting
concurrently, this attribution systematically broke down — made worse
by the converter nudging identical timestamps within a stream apart by
1ns, so `delay_task` and `delay_ticks`, for example, no longer landed
on exactly the same timestamp. This produced hundreds of false
violations in `delay.tessla` and `semaphore.tessla`, even though the
kernel was working correctly. The converter now produces one stream
per task (`delay_ticks_0` … `delay_ticks_3`, likewise for `mtx_*`,
`sem_*`, `q_*_block_ticks`); the attribution is thereby structurally
unambiguous, and the filter logic in the specs is no longer needed.

### 3.5 Bugs in the Test Code Itself

The integration tests provoke genuine concurrency — and in doing so
exposed four bugs *in the test code*. The kernel behaved correctly in
all four cases:

1. **Live-lock from too short a poll interval.** `prv_vWaitForPhase()`
   polled with `Scheduler_vNonBlockedDelay(1u)`. The high-priority
   `TestHighTask` (prio 3) woke up again almost immediately after one
   tick and permanently beat `TestMainTask` (prio 1) — which was the
   one that first had to set the phase — to the punch. The poller
   starved exactly the task it was waiting on. Fixed with a poll
   interval of 10 ticks.
2. **Swapped phase order.** `TestMainTask` set the phases in the order
   …7 → 11 → 9, but `TestPeerTask` waited in the order …6 → 9 → 11. At
   T16, Main blocked in `SendBlocking` on the full queue while Peer was
   waiting on phase 9 and never saw the phase 11 that had been set:
   deadlock.
3. **Stale result inside the infinite loop.** The return value of
   `prv_u8WaitFlag()` was discarded and `s_ePeerResult` was evaluated
   afterward regardless. If the peer ran into a timeout, the value came
   from the **previous round** — the test would have falsely reported
   `[OK]`. The wait result is now factored into the evaluation.
4. **Phase values reused across rounds.** After switching to an
   infinitely repeated test sequence (see 2.4), the same phase numbers
   (2, 3, 4, 6, 7, 9, 11) recurred every round. A task waiting early in
   its loop on a low phase number could therefore not distinguish
   whether that phase had just been set or still belonged to the
   *previous* round — it would then wrongly proceed immediately and run
   ahead of the current round, which reproducibly starved
   `TestMainTask` under the added load of SystemView tracing (over
   UART, i.e. without tracing overhead, the timing window was tight
   enough that it never occurred). Fixed with a round counter
   (`s_u16Round`) incremented on every pass; every wait point now
   compares phase **and** round.

### 3.6 Wrong Tick Length in the TeSSLa Specs

All six specs assumed `TICK = 1ms` (the default assumption for a
SysTick-based scheduler). After fixing the ticks attribution (3.4),
`delay.tessla` and `semaphore.tessla` still had violations left over —
with conspicuously regular, repeating timestamps. A timeout test case
(`OS_Semaphore_TakeTimeout(&sem, 20u)`) fired after 1.6ms instead of
the expected 20ms.

Measuring empirically directly from the trace (gap between consecutive
`Task Stop` events of the idle task, which coincide exactly with each
SysTick) gave an actual tick of **80µs**, not 1ms — a factor of 12.5.
`20 ticks × 80µs = 1.6ms` matches the measured time exactly. The
scheduler counts down correctly in its own ticks; the wrong assumption
was purely in the specs. `TICK` was corrected to `80000` (ns) in all
six files; `WARMUP` was at the same time deliberately switched from a
`TICK` multiplication to a fixed value (250ms), so a future tick
correction does not accidentally also shorten the state
reconstruction's settling time.

In the same pass, two further spots were hardened that had gone
unnoticed under the old, wrong time base: `earlyWake` (delay.tessla)
and `earlyTmo` (semaphore.tessla) now additionally check whether an
*actually still-open* block/delay episode existed for the task in
question before forming a time difference. Without this check, an
internal fallback value (`-1000000000`) produced an artificially huge
difference when no prior episode existed, causing isolated false
violations.

**Open question:** whether the 80µs tick is a deliberate design choice
(finer scheduling granularity) or a SysTick reload-value
misconfiguration was not conclusively determined.

---

## 4. Verification Chain

```
SystemView recording
   │  export as CSV
   ▼
sysview_to_tessla_bytes.py          (VarUint32 decoding)
   │  <timestamp_ns>: <stream> = <value>
   ▼
java -jar tessla.jar interpreter <spec>.tessla trace.input
   │
   ▼
empty violation_* streams = rule upheld
```

Six specifications in `tessla/`, five checked against real recordings:

| Spec | Covers | Required trace groups | Status |
|---|---|---|---|
| `scheduler_tasks.tessla` | Scheduler and task rules (10 requirements) | SCHEDULER + ISR | passed |
| `mutex.tessla` | 6 mutex rules | SCHEDULER + MUTEX | passed |
| `semaphore.tessla` | 7 semaphore rules | SCHEDULER + SEMAPHORE | passed |
| `queue.tessla` | 9 queue rules (FIFO/data integrity separate, see below) | SCHEDULER + QUEUE | passed |
| `delay.tessla` | 4 delay rules | SCHEDULER + DELAY + block events | passed |
| `sensor.tessla` | 100ms transmission cadence | APP | written, not yet finally checked against a trace |

`scheduler_tasks.tessla` needs a trace with the `OS_TRACE_ISR` group
active; the other four ran with this group disabled (smaller,
overflow-safe trace) — both recordings are part of the submission.

**Encoding pattern of the specs.** State is first reconstructed from
the raw events (task state, mutex owner, semaphore value, queue fill
level); invariants and obligations are then checked on top of that.
"After X, a Y must follow within T" is encoded as an open obligation,
checked for overdue-ness against the dense `exec` heartbeat (the
scheduler fires every tick) — pure past-time evaluation, with no
future operators.

**Tick length.** `TICK = 80000` (ns) — measured empirically from the
trace, see 3.6. Not the 1ms initially assumed.

**Warm-up window.** All specs ignore the first 250ms (a fixed value,
independent of `TICK`): recording starts mid-flight, and the
reconstructed state needs some real time before it matches reality.

**Per-task ticks attribution.** Wait durations (`Ticks`) for delays,
semaphore blocks, and queue blocks run over their own stream per task
(`delay_ticks_0` … `_3` etc., see 3.4) rather than over a shared stream
with post-hoc filtering.

**FIFO order and unmodified content (queue)** are commented out in the
spec: the list-based checksum reconstruction blows up the Java heap in
the TeSSLa interpreter (`OutOfMemoryError`, reproducible even with
`-Xmx8g`) once the number of queue operations reaches the low
double digits. Both properties are instead demonstrated functionally
via integration test T18 (16 messages with a known checksum pattern
through a queue smaller than the message count).

### Practical Workflow

```bash
# Test mode for mutex/semaphore/delay/queue (genuine contention)
#    os_trace_config.h: OS_RUN_INTEGRATION_TESTS = 1, OS_TRACE_ISR = 0
python3 tools/sysview_to_tessla_bytes.py test.csv -o t.input
java -jar tessla.jar interpreter tessla/mutex.tessla     t.input
java -jar tessla.jar interpreter tessla/semaphore.tessla t.input
java -jar tessla.jar interpreter tessla/delay.tessla     t.input
java -jar tessla.jar interpreter tessla/queue.tessla     t.input

# Separate run for scheduler/tasks (needs OS_TRACE_ISR = 1)
python3 tools/sysview_to_tessla_bytes.py isr_test.csv -o i.input
java -jar tessla.jar interpreter tessla/scheduler_tasks.tessla i.input

# Normal operation for sensor.tessla
#    os_trace_config.h: OS_RUN_INTEGRATION_TESTS = 0
python3 tools/sysview_to_tessla_bytes.py normal.csv -o n.input
java -jar tessla.jar interpreter tessla/sensor.tessla n.input
```

---

## 5. Operation

**Terminal:** 115200 8N1 on the ST-LINK Virtual COM Port.

```
help          command overview
status        show calibration values
cal <mm>      start calibration (hold an object still at <mm> distance)
```

Calibration averages 8 valid raw readings and sets
`Offset = target value − average`. During this phase, distance output
pauses; `sensor.tessla` deliberately excludes this window
(`CalStart` … `CalDone`) when checking the cadence.

## 6. Generating the Documentation

```bash
doxygen Doxyfile      # → doc/html/index.html
```

The `Doxyfile` is configured to report missing documentation as a
warning (`doc/doxygen_warnings.txt`); the current state is
warning-free. Call and include graphs require Graphviz.

## 7. Known Limitations

- **Idle events are missing** (see 2.1) — a deliberate trade-off in
  favor of a stable trace.
- **Timeout semantics:** the timeout clock only runs while BLOCKED. A
  task that gets woken, loses the retry race, and stays READY for a
  while has its clock paused — the real wait time can exceed the
  nominal timeout. The specs account for this with generous upper
  bounds.
- **Barging:** if the previous owner releases a mutex and immediately
  re-requests it, it can get it back ahead of the woken waiters. The
  priority rule holds at tick granularity.
- **Module description shortened** (see 2.2) — in SystemView,
  non-mutex events appear as `Function #NNN`. Irrelevant for
  verification, since decoding is done in binary.
