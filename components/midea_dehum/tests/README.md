# Component Tests

Host tests compile the component with mock ESPHome headers. No device is needed.

From the repository root:

```bash
make -C components/midea_dehum/tests test
```

## Targets

| Target | Action |
| --- | --- |
| `all` (default) | Build the combined harness |
| `test`, `test-all`, `run-all` | Run all tests, including protocol detection |
| `run-v1`, `run-v2`, `run-v3` | Run the same shared suite, without detection tests |
| `standalone` | Build each suite as a `test_*_solo` executable |
| `test-asan` | Rebuild with AddressSanitizer and run all tests |
| `clean` | Remove test binaries, objects, and dependency files |

## Test Files

| File | Coverage |
| --- | --- |
| `test_handshake.cpp` | Startup, handshake, disabled handshake, early status |
| `test_commands.cpp` | Controls, captured V3 frames, sequence matching |
| `test_e2e.cpp` | Power cycles, mode/fan changes, status updates |
| `test_system.cpp` | Invalid frames, checksums, state parsing |
| `test_harness.cpp` | Combined runner, protocol detection and switching |
| `fixtures.h` | Test device and captured frames |
| `mock_esphome.h` | Mock UART, scheduler, and ESPHome entities |

## Add a Test

Add a test to the relevant suite and register it in `test_harness.cpp`.
If the suite has a standalone runner, register it there too. Use the fixtures
and assertion helpers, then run `make test`.

The Makefile tracks included files, so source and header edits trigger a rebuild.
After changing compiler flags, run `make clean` before rebuilding.
