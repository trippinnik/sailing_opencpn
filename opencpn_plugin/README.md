# OpenCPN Sailing Plugin

Collects live sailing data from OpenCPN (via NMEA sentences) and
broadcasts it as JSON over UDP so that a phone companion app can relay
it to a Garmin watch running the **SailingApp** MonkeyC application.

## NMEA sentences parsed

| Sentence | Data extracted |
|----------|----------------|
| `DPT` / `DBT` | Water depth (m) |
| `VTG` | SOG (kn), COG (°T) |
| `RMC` | SOG (kn), COG (°T) – fallback |
| `MWV` | Apparent wind speed & angle; true wind speed & direction |
| `MWD` | True wind direction & speed |

## UDP broadcast format

The plugin broadcasts a compact UTF-8 JSON object once per second to
`255.255.255.255:55554` (configurable via the preferences dialog).

```json
{
  "sog":   5.2,
  "cog":   245.0,
  "depth": 12.5,
  "aws":   15.0,
  "awa":   135.0,
  "tws":   12.0,
  "twd":   220.0
}
```

Only fields with valid data are included.

## Building

### Prerequisites

* CMake ≥ 3.10
* wxWidgets ≥ 3.0 (development headers)
* OpenCPN headers (`ocpn_plugin.h`) – usually in `/usr/include/opencpn`

### Linux / macOS

```bash
cd opencpn_plugin
mkdir build && cd build
cmake .. -DOPENCPN_INCLUDE_DIR=/usr/include/opencpn
make -j$(nproc)
sudo make install      # installs to /usr/lib/opencpn/
```

### Windows (MinGW / MSVC)

```powershell
cd opencpn_plugin
cmake -B build -DOPENCPN_INCLUDE_DIR="C:\Program Files\OpenCPN\include"
cmake --build build --config Release
cmake --install build
```

## Preferences

Open **Options → Plugins → Sailing → Preferences** to change the
broadcast address and port (default `255.255.255.255:55554`).

## Data flow architecture

```
Instruments ──NMEA──► OpenCPN ──plugin──► UDP JSON broadcast
                                                   │
                                            phone on same Wi-Fi
                                                   │
                                        companion app (bridge)
                                                   │ BLE
                                            Garmin watch
                                                   │
                                          SailingApp (MonkeyC)
```

The companion app listens on UDP port 55554 and forwards each JSON
payload to the watch app via `Communications.transmit()` (Garmin
Connect IQ SDK phone-side API).
