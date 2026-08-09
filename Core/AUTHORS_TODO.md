# Autorenschaft eintragen

In allen von euch geschriebenen Dateien steht als Platzhalter `__________`
hinter jedem `@author`. Diese müsst ihr noch durch eure Namen ersetzen —
einmal im Dateikopf und einmal pro Funktion.

## Alle offenen Stellen finden

```bash
grep -rn "__________" Core/Inc Core/Src
```

## Ersetzen

Wenn eine ganze Datei von einer Person stammt:

```bash
sed -i 's/__________/Max Mustermann/g' Core/Src/scheduler.c
```

Für geteilte Leistung an einzelnen Funktionen bietet sich an:

```
 * @author  Max Mustermann, Erika Musterfrau
 * @note    Shared work
```

## Bearbeitete Dateien (31)

### Eigene Module — vollständig
Inc: app_messages.h, app_resources.h, board_config.h, hcsr04.h, os_common.h,
os_mutex.h, os_queue.h, os_semaphore.h, os_trace.h, os_trace_config.h,
scheduler.h, shell.h, stack.h, tasks.h, tests.h, uart_driver.h

Src: app_resources.c, hcsr04.c, os_mutex.c, os_queue.c, os_semaphore.c,
os_trace.c, pendsv.s, scheduler.c, shell.c, stack.c, tasks.c, tests.c,
uart_driver.c

### CubeMX-Gerüst, eigene USER-CODE-Abschnitte
- `Src/main.c` — Header vermerkt die Abgrenzung; `@author` nur an den
  Funktionen, die ihr geschrieben habt
- `Src/stm32l4xx_it.c` — `@author __________ (USER CODE sections only)`;
  dokumentiert sind SysTick-Hook, EXTI0 (Echo) und USART1

## Bewusst NICHT angefasst
- `Inc/tcb.h` — Vorlage vom Dozenten (David)
- CubeMX-generiert: gpio, i2c, spi, quadspi, dfsdm, usb_otg,
  stm32l4xx_hal_msp, syscalls, sysmem, system_stm32l4xx, main.h,
  stm32l4xx_hal_conf.h, stm32l4xx_it.h und die zugehörigen Header
- `Segger/` — SEGGER SystemView/RTT (Fremdcode)
- `Startup/` — ST-Startupcode

Hier solltet ihr **keine** Autorenangabe eintragen.
