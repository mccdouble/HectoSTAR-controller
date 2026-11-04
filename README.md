# HectoSTAR Controller

Open Ephys plugin for the HectoSTAR system: streams data from the Galvani3 (HectoSTAR chronic) chip or Intan RHD headstages (acute) through an Opal Kelly XEM7310 FPGA, and drives Galvani3 LED stimulation (pulse and pattern output).

It builds `HectoSTAR-controller.dll` and registers the **HectoSTAR Controller** source and **HectoSTAR Output** sink, so it can be installed alongside the stock RHD Recording Controller without conflict.

Derived from the Open Ephys [RHD Recording Controller](https://github.com/open-ephys-plugins/rhd-recording-controller) plugin (GPL-3.0; see `LICENSE`).

## Building from source

First, follow the instructions on [this page](https://open-ephys.github.io/gui-docs/Developer-Guide/Compiling-the-GUI.html) to build the Open Ephys GUI.

Then place this repository next to `plugin-GUI`:

```
OpenEphys V1.0.2
├── plugin-GUI
└── OEPlugins
    └── HectoSTAR-controller
        ├── Build
        ├── Resources
        └── Source
```

### Windows

**Requirements:** [Visual Studio](https://visualstudio.microsoft.com/) and [CMake](https://cmake.org/install/)

From the `Build` directory:

```bash
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

Or open `OE_PLUGIN_HectoSTAR-controller.sln` in Visual Studio. Building the `INSTALL` project copies the `.dll`, `okFrontPanel.dll` and `Galvani3.bit` into the GUI's `plugins` and `shared` directories.

### Linux

From the `Build` directory:

```bash
cmake -G "Unix Makefiles" ..
make -j
make install
```

### macOS

From the `Build` directory:

```bash
cmake -G "Xcode" ..
```

Then open the generated Xcode project and run the `INSTALL` scheme.
