/*
 * hcsr04.c
 *  Interrupt-basierter Treiber fuer den HC-SR04 Ultraschallsensor.
 *
 *  Zeitmessung: DWT->CYCCNT (CPU-Zyklenzaehler). Die Umrechnung in µs
 *  erfolgt ueber SystemCoreClock - der Treiber funktioniert damit bei
 *  jedem Systemtakt (48 MHz, 72 MHz, ...) korrekt. Der Zaehler laeuft
 *  frei durch; die unsigned-Differenz (Ende - Start) ist auch beim
 *  Ueberlauf korrekt. WICHTIG: CYCCNT nie nullen - SystemView nutzt
 *  denselben Zaehler fuer seine Zeitstempel!
 *
 *  Umrechnung: mm = pulse_us * 100 / 583   (Schallgeschwindigkeit 343 m/s,
 *  Hin- und Rueckweg -> us / 5.83 = mm)
 */

#include "hcsr04.h"
#include "board_config.h"
#include "app_resources.h"
#include "os_semaphore.h"

/* Messzustand - nur ISR und Sensor-Task greifen zu (Task liest erst
 * nach dem Semaphore-Take, die ISR schreibt davor -> keine Races) */
static volatile uint32_t s_u32RiseCycles  = 0u;   ///< CYCCNT bei steigender Flanke
static volatile uint32_t s_u32PulseCycles = 0u;   ///< Pulsdauer in Zyklen
static volatile uint8_t  s_u8Armed        = 0u;   ///< Messung scharf?
static volatile uint8_t  s_u8GotRise      = 0u;   ///< steigende Flanke gesehen?

void HCSR04_vInit(void)
{
    /* DWT-Zyklenzaehler defensiv aktivieren (ohne Reset!) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    /* EXTI-Prioritaet 5: unter SysTick (0), ueber UART (6) -
     * das Echo-Timing ist das zeitkritischste Signal im System. */
    HAL_NVIC_SetPriority(HCSR04_ECHO_IRQn, 5u, 0u);
    HAL_NVIC_EnableIRQ(HCSR04_ECHO_IRQn);

    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

/// Busy-Wait in Mikrosekunden ueber den Zyklenzaehler (nur fuer den 10us-Trigger)
static void prv_vDelayUs(uint32_t us)
{
    uint32_t u32Start  = DWT->CYCCNT;
    uint32_t u32Cycles = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - u32Start) < u32Cycles) { }
}

void HCSR04_vStartMeasurement(void)
{
    s_u8GotRise = 0u;
    s_u8Armed   = 1u;

    /* 10us-Triggerpuls laut Datenblatt */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    prv_vDelayUs(10u);
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

void HCSR04_vEchoEdgeIsr(void)
{
    if (s_u8Armed == 0u)
    {
        return;   /* Flanke ohne laufende Messung -> ignorieren */
    }

    if (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
    {
        /* Steigende Flanke: Echo beginnt */
        s_u32RiseCycles = DWT->CYCCNT;
        s_u8GotRise     = 1u;
    }
    else if (s_u8GotRise != 0u)
    {
        /* Fallende Flanke: Echo endet -> Pulsdauer festhalten,
         * Messung entschaerfen und den Sensor-Task aufwecken */
        s_u32PulseCycles = DWT->CYCCNT - s_u32RiseCycles;
        s_u8Armed        = 0u;
        s_u8GotRise      = 0u;
        OS_Semaphore_Give(&g_echoDoneSemaphore);
    }
}

uint32_t HCSR04_u32GetPulseUs(void)
{
    return s_u32PulseCycles / (SystemCoreClock / 1000000u);
}

uint32_t HCSR04_u32GetDistanceMmRaw(void)
{
    return (HCSR04_u32GetPulseUs() * 100u) / 583u;
}

uint8_t HCSR04_u8IsOutOfRange(void)
{
    uint32_t u32Us = HCSR04_u32GetPulseUs();
    return (u32Us < HCSR04_PULSE_MIN_US || u32Us > HCSR04_PULSE_MAX_US) ? 1u : 0u;
}
