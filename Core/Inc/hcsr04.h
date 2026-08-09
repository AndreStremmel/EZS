/*
 * hcsr04.h
 *  Interrupt-basierter Treiber fuer den HC-SR04 Ultraschallsensor.
 *
 *  Ablauf einer Messung (aus dem Sensor-Task):
 *   1. HCSR04_vStartMeasurement()  -> 10us-Triggerpuls, Messung "scharf"
 *   2. OS_Semaphore_TakeTimeout(&g_echoDoneSemaphore, HCSR04_ECHO_TIMEOUT_TICKS)
 *      - OS_OK:      Echo-Puls vollstaendig vermessen (EXTI-ISR hat Give gemacht)
 *      - OS_TIMEOUT: kein/unvollstaendiges Echo -> Sensorfehler
 *   3. HCSR04_u32GetDistanceMmRaw() -> unkalibrierte Distanz in mm
 */

#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>

/// Max. Wartezeit auf das Echo in Ticks (HC-SR04: max ~25ms Puls + Reserve)
#define HCSR04_ECHO_TIMEOUT_TICKS   ( 40u )

/// Plausibilitaetsgrenzen der Pulsdauer (Datenblatt: ~150us..25ms)
#define HCSR04_PULSE_MIN_US         ( 100u )
#define HCSR04_PULSE_MAX_US         ( 30000u )

/// DWT-Zykluszaehler sicherstellen (SystemView aktiviert ihn i.d.R. schon,
/// wir schalten defensiv nach - OHNE den Zaehler zu nullen!)
void HCSR04_vInit(void);

/// Triggerpuls senden und Flankenerfassung scharf schalten
void HCSR04_vStartMeasurement(void);

/// Gemessene Echo-Pulsdauer in Mikrosekunden (nach erfolgreichem Take)
uint32_t HCSR04_u32GetPulseUs(void);

/// Unkalibrierte Distanz in mm (aus der letzten Pulsdauer)
uint32_t HCSR04_u32GetDistanceMmRaw(void);

/// 1 wenn die letzte Pulsdauer ausserhalb der Plausibilitaetsgrenzen lag
uint8_t HCSR04_u8IsOutOfRange(void);

/// Aus HAL_GPIO_EXTI_Callback aufrufen, wenn der ECHO-Pin die Flanke ausloest
void HCSR04_vEchoEdgeIsr(void);

#endif /* HCSR04_H */
