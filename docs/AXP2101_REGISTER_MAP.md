# AXP2101 Register Map Reference

**Source**: AXP2101-PMU-Datasheet.pdf

## Status Registers

| Register | Address | Description | Notes                                            |
| -------- | ------- | ----------- | ------------------------------------------------ |
| REG 00   | 0x00    | PMU status1 | VBUS/ACIN/VBAT present, bit 5: VBUS present      |
| REG 01   | 0x01    | PMU status2 | Charge status (bit 6: charging - inverted logic) |

## Control Registers

| Register | Address | Description                                  | Bits                                                                                                  |
| -------- | ------- | -------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| REG 18   | 0x18    | Charger, fuel gauge, watchdog on/off control | Bit 3: Gauge enable, Bit 2: Button Battery charge, Bit 1: Cell Battery charge enable, Bit 0: Watchdog |

## ADC Registers

| Register | Address | Description                | Format                                 |
| -------- | ------- | -------------------------- | -------------------------------------- |
| REG 30   | 0x30    | ADC Channel enable control | Bit enables for various ADC channels   |
| REG 34   | 0x34    | vbat_h                     | Battery voltage high byte (14-bit ADC) |
| REG 35   | 0x35    | vbat_l                     | Battery voltage low byte (1mV/LSB)     |
| REG 38   | 0x38    | vbus_h                     | VBUS voltage high byte (14-bit ADC)    |
| REG 39   | 0x39    | vbus_l                     | VBUS voltage low byte (1mV/LSB)        |
| REG 3A   | 0x3A    | vsys_h                     | System voltage high byte (NOT VBUS!)   |
| REG 3B   | 0x3B    | vsys_l                     | System voltage low byte                |
| REG 3C   | 0x3C    | tdie_h                     | Die temperature high byte              |
| REG 3D   | 0x3D    | tdie_l                     | Die temperature low byte               |

## Current Implementation Status

### ✅ Correct Registers

- Status register: 0x00
- Charge status: 0x01
- Charge config: 0x18 (bit 1 for Cell Battery charge enable)
- ADC enable: 0x30
- Battery voltage: 0x34-0x35
- **VBUS voltage: 0x38-0x39** (NOT 0x3A-0x3B!)

### ❌ Registers That Need Verification

- Fuel gauge percentage: Currently using 0xA4 - NEEDS VERIFICATION FROM DATASHEET

## Notes

1. **REG 18 bit mapping** (page 28):

   - Bit 3: Gauge Module enable (0: disable, 1: enable)
   - Bit 2: Button Battery charge enable
   - Bit 1: Cell Battery charge enable (THIS IS WHAT WE USE)
   - Bit 0: Watchdog Module enable

2. **ADC voltage format**: 14-bit value, 1mV per LSB

   - Read 2 bytes: `voltage_mv = (high_byte << 8) | low_byte`

3. **Charge status logic** (REG 01, bit 6): INVERTED

   - Bit 6 = 0: Charging
   - Bit 6 = 1: Not charging

4. **VBUS vs VSYS**:
   - VBUS (0x38-0x39): USB input voltage
   - VSYS (0x3A-0x3B): System power rail voltage
