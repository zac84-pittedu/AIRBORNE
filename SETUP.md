# Headless 12-device chronoamperometry logger (Raspberry Pi Zero W, Pi OS Trixie)

Holds 12 CNT-on-interdigitated-electrode devices at a constant +0.5 V and logs
their current once per second to a CSV file. No screen, no GUI: it starts on
boot, blinks the onboard LED to show it is running, and writes crash-safely.

## Files

- `var.py`      - I2C/SPI setup + all register/hardware constants (bias, gain, addresses)
- `settings.py` - low-level helpers (LMP register writes, ADC read, mux select, conversion)
- `logger.py`   - the main loop (run this)
- `chrono-logger.service` - systemd unit for auto-start on boot

Put the three `.py` files in one folder (the service assumes `/home/pi/chrono` -
edit the unit if you use a different path).

## Hardware map (as wired)

- LMP91000EVM: I2C @ 0x48, SPI on SPI0/CE0. 2-WIRE jumper set. Op-mode 3-lead amperometric.
- MCP23017:    I2C @ 0x27 (A2/A1/A0 all high). VCC = 3.3 V. Port A bits 0-3 -> mux S0-S3.
- CD74HC4067:  VCC = 3.3 V, EN tied to GND (always enabled). Common line -> LMP WE/TIA input.
               Channels 0-11 -> the 12 devices. All devices share the always-on 0.5 V bias.
- Bias/gain:   +0.5 V (REFCN 10111011), R_TIA = 7 kOhm. Both tunable in var.py.

## One-time Pi setup

    sudo apt update
    sudo apt install python3-smbus python3-spidev i2c-tools

    sudo raspi-config     # Interface Options -> enable I2C AND SPI, then reboot
    sudo reboot

Confirm both chips are on the bus (with the LMP connected):

    sudo i2cdetect -y 1   # expect 0x27 (MCP) and 0x48 (LMP)

Note: numpy / matplotlib / tkinter are NOT needed by this headless version.

## Test run (before enabling the service)

    cd /home/pi/chrono
    sudo python3 logger.py

It prints the output filename and starts scanning; the onboard LED blinks ~1 Hz.
Ctrl-C stops it cleanly (file closed, front end parked, LED restored). Check the
newest file in `Results/` and confirm 26 columns and one row per second.

## Auto-start on boot

    sudo cp chrono-logger.service /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable chrono-logger.service    # start at every boot
    sudo systemctl start  chrono-logger.service    # start now

    systemctl status chrono-logger.service         # check state
    journalctl -u chrono-logger.service -f         # watch its output live

To stop: `sudo systemctl stop chrono-logger.service` (this closes the file
cleanly). To disable auto-start: `sudo systemctl disable chrono-logger.service`.

## Output format

One timestamped file per run: `Results/chrono_YYYY-MM-DD_HHMMSS.csv`, with a
header row, then one row per scan:

    iso_timestamp, elapsed_s, dev00_raw, dev00_current_uA, ... dev11_raw, dev11_current_uA

Each device value per row is the MEAN of all ADC reads taken during a ~75 ms
window for that device (mean raw ADC and the current computed from it). The
number of reads averaged depends on the board's per-read speed (tens to a couple
hundred on a Zero W) and is printed once at startup. Individual reads are not
stored. Note when reporting: one point/device/second = mean of ~N reads.

- `elapsed_s` comes from a monotonic clock and is always reliable.
- `iso_timestamp` is wall-clock time (see clock caveat below).
- Each row is flushed and fsync'd to the SD card immediately, so an unexpected
  power loss costs at most the row in progress, never the whole run.

## Things to know / tune

- CURRENT OFFSET: the raw->current formula is carried over unchanged from the
  original project. At raw = 0 it reports ~+178 uA, not 0 - there is a fixed
  offset baked into the convention. For relative-change sensing this is
  irrelevant (it cancels in dI). Because raw ADC is logged alongside current,
  you can recompute absolute current however you like later. During bring-up,
  it is worth checking the scaling against a known resistor.

- TIA GAIN: 7 kOhm is the starting guess. If the raw column pins near its max
  (~+/-32767) you are saturating - drop to a lower gain by changing TIA_SETTING
  and R_TIA (keep them consistent) in var.py. If raw barely moves off the noise
  floor, raise the gain.

- TIMING: in logger.py. Each device is settled (SETTLE_S = 3 ms) then read
  back-to-back for AVG_WINDOW_S = 75 ms and all reads averaged. (SETTLE + WINDOW)
  x 12 ~= 0.94 s, leaving margin in the 1 s interval. The window length rejects
  50/60 Hz mains pickup; the read COUNT within it cuts random noise by ~sqrt(N).
  Raise AVG_WINDOW_S toward 0.08 to use more of the second; lower it for a faster
  cadence. Startup prints how many reads/device actually fit on your board -- if
  you want to check that number directly, time readadc() in a loop first.
  Devices are biased continuously, so SETTLE_S can likely drop toward 0.

- CLOCK: the Zero W has no real-time clock. Deployed without network, the
  wall-clock timestamp will be whatever fake-hwclock restored at boot until (if
  ever) NTP corrects it. This does NOT affect syncing with other sensors on the
  SAME Pi - they all share the same system clock, so relative alignment holds,
  and elapsed_s is exact regardless. Only ABSOLUTE wall time is uncertain. If
  you need trustworthy absolute timestamps offline, add a small RTC module.

- RESILIENCE: a hard crash triggers a systemd restart into a NEW file (small
  gap, no data lost). If you see transient I2C read glitches in the field, we
  can add per-read error handling so one bad read never interrupts a run - easy
  add, left out for now to keep the loop simple.
