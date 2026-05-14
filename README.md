# sailing_opencpn

Display real-time sailing data from **OpenCPN** on a **Garmin ConnectIQ** watch.

## What's in this repository

| Directory | Description |
|-----------|-------------|
| `garmin_app/` | Garmin ConnectIQ watch app written in **MonkeyC** |
| `opencpn_plugin/` | OpenCPN plugin written in **C++** that feeds the watch |

---

## System architecture

```
Marine instruments
       │ NMEA 0183 / 2000
       ▼
  OpenCPN (chartplotter)
       │ NMEA sentences (DPT, VTG, MWV, …)
       ▼
  sailing_pi (OpenCPN plugin)
       │ JSON over UDP broadcast  (port 55554)
       ▼
  Phone companion app  ◄── same Wi-Fi network
       │ BLE  (Garmin Connect IQ phone SDK)
       ▼
  Garmin watch  ──  SailingApp (MonkeyC)
```

### Data fields

| Field | Description | Unit |
|-------|-------------|------|
| `sog` | Speed Over Ground | knots |
| `cog` | Course Over Ground | degrees true |
| `depth` | Water depth | metres |
| `aws` | Apparent Wind Speed | knots |
| `awa` | Apparent Wind Angle | degrees (port = negative) |
| `tws` | True Wind Speed | knots |
| `twd` | True Wind Direction | degrees true |

---

## Garmin watch app (`garmin_app/`)

Written with the **Garmin Connect IQ SDK** (MonkeyC language).

### Features

* **Page 0** – SOG and COG in large, easy-to-read numerals
* **Page 1** – Water depth and apparent / true wind speed & angle
* Stale-data indicator (values dim when no update for > 30 s)
* Page-dot navigation; swipe or press UP/DOWN to switch pages
* Adapts to any Garmin watch screen shape and resolution

### Build requirements

* [Garmin Connect IQ SDK](https://developer.garmin.com/connect-iq/sdk/) ≥ 4.x
* MonkeyC extension for VS Code **or** the `monkeyc` CLI

### Build (CLI)

```bash
cd garmin_app
monkeyc -f monkey.jungle -o SailingApp.prg -d fenix6
```

### Supported devices

Any device listed in `manifest.xml`; add more `<iq:product>` entries as
needed.

---

## OpenCPN plugin (`opencpn_plugin/`)

See [`opencpn_plugin/README.md`](opencpn_plugin/README.md) for full build
and installation instructions.

### Quick start (Linux)

```bash
cd opencpn_plugin
mkdir build && cd build
cmake .. -DOPENCPN_INCLUDE_DIR=/usr/include/opencpn
make -j$(nproc)
sudo make install
```

Restart OpenCPN and enable the **Sailing** plugin under
*Options → Plugins*.

---

## License

See [LICENSE](LICENSE).
