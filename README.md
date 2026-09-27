# rssi-probe

Standalone portable Windows tool: watch live Bluetooth signal (RSSI) values
from your headphones and see what the movement detector makes of them —
built to tune the Adaptive Volume thresholds in
[vkhawse2/Sound-connect](https://github.com/vkhawse2/Sound-connect).

No install needed: unzip and run `rssi-probe.exe`.

## What it does

- Discovers nearby BLE devices and lets you pick one to watch.
- Shows live RSSI readout plus variance / sample-rate / min-max stats.
- Shows the `MovementDetector` verdict (Still / Moving / Detecting) with
  live-tunable thresholds, so candidate values can be validated against
  real data before changing the app.
- Scrolling log of raw samples.

## Build

Needs Qt 6 (Core, Gui, Widgets). On Windows the BLE parts use C++/WinRT;
on other platforms the watcher is inert but the tool still builds.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target rssi-probe --parallel
```

## Release

Push a tag like `v0.1` — the `RSSI Probe` workflow builds the portable
Windows ZIP and publishes it as a GitHub release. A manual
`workflow_dispatch` run builds without publishing; its `manual-publish`
job can attach an earlier run's artifact to a release if the tag-triggered
publish ever fails.

## Provenance

Extracted from `vkhawse2/Sound-connect` (`tools/rssi-probe`) on 2026-09-27.
`MovementDetector.*` / `RssiWatcher.*` are vendored snapshots of the app
sources at extraction time — this repo has no dependency on the
Sound-connect tree. If the app's detector changes and you want the probe
to match, copy the four files over again.

## Support

If this tool is useful to you, consider [sponsoring on
GitHub](https://github.com/sponsors/vkhawse2). GitHub charges no fees on
personal sponsorships, so 100% of your support goes to development.
