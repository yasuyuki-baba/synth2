# Product resources

| File | Purpose |
| --- | --- |
| init.sy1 | Initialization patch, embedded in the portable core |
| parameter_names.json | 128 display labels in source parameter order (p0 through p127), shared by the core and editor preview |

Display labels do not define parameter ranges, defaults, source-version conversion
or DSP behavior. Those belong to the product loading/host adapters and their tests.
The separate masterVolume host parameter has its label in ProcessorBridge.

The embedding tool and CMake dependencies consume these files directly. Keep
initial patch bytes and label ordering stable unless the product contract
explicitly changes. Native format metadata and bundled fonts live under
projects/Synth2/resources.

No bank is distributed or embedded. The library starts empty; users add banks
and patches through the same import path. Init remains the independent startup
sound rather than a library entry.
