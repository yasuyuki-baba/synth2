# Installing Synth2

Choose the native package for your operating system:
`Synth2-linux.tar.gz`, `Synth2-windows.zip` or `Synth2-macos.tar.gz`. GitHub's source archives are for
building Synth2, not installing it. The current workflow uploads candidates to
GitHub Actions; if a version is published under
[Releases](https://github.com/yasuyuki-baba/synth2/releases), use its native assets.
Downloading an Actions artifact may require signing in to GitHub. An Actions
artifact ZIP wraps the native package: extract that outer ZIP first, then extract
the native ZIP or tar.gz inside it.

The extracted native package contains a `Synth2` folder with the plugin products,
standalone application, `dependencies.json`, `LICENSE` and `notices/`.
Products are directly inside `Synth2/`, not in an `out/` subfolder.
`dependencies.json` identifies the source and SDK revisions for that build.

Close your DAW and Synth2 before installing or updating. Copy each complete
product, keeping its original name and contents. When updating, replace the old
bundle rather than merging folders. Install only formats your DAW supports.
Keep the package's license and notice files with your downloaded distribution.

## Linux

Linux packages target x86_64 and are built on Ubuntu 24.04 (glibc 2.39). Use that
release or a compatible newer distribution; ARM Linux and older glibc systems are
not covered by these packages. Extract the tar.gz on Linux to preserve executable
permissions and symlinks. The editor uses X11 and OpenGL 3; Wayland sessions need
XWayland. Runtime libraries include GTK3, FreeType/fontconfig, X11/OpenGL, ALSA,
JACK and PulseAudio. On Ubuntu 24.04, install missing runtime dependencies with:

```sh
sudo apt-get install libgtk-3-0t64 libasound2t64 libjack-jackd2-0 libpulse0 libgl1
```

| Product in `Synth2/` | Destination folder |
| --- | --- |
| `Synth2.vst3` (entire bundle) | `~/.vst3/` |
| `Synth2.clap` | `~/.clap/` |
| `Synth2` (standalone) | A folder you own, such as `~/.local/opt/Synth2/` |

Create missing folders. Keep any `Synth2.clap.resources` directory beside
`Synth2.clap`, and any `Synth2.resources` directory beside the standalone binary.
Keep the VST3 bundle intact, including `Contents/x86_64-linux/Synth2.so` and its
resources. Run the standalone binary from its extracted folder; audio requires a
working ALSA device or JACK server. Use the portable source build on platforms
outside this binary baseline; see the [build guide](docs/desktop-build.md).

## macOS

Current macOS candidates require Apple Silicon (arm64) and macOS 13 or later.
Intel Macs are not supported. In Finder, choose **Go > Go to Folder…** and
enter a destination below. `~` means your home folder. Create missing folders;
these user-level locations do not require administrator permission.

| Product in `Synth2/` | Destination folder |
| --- | --- |
| `Synth2.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `Synth2.clap` | `~/Library/Audio/Plug-Ins/CLAP/` |
| `Synth2.component` (AUv2) | `~/Library/Audio/Plug-Ins/Components/` |
| `Synth2.app` (standalone) | `~/Applications/` |

Copy the entire bundle into its destination. For example, the installed VST3
should be `~/Library/Audio/Plug-Ins/VST3/Synth2.vst3`. AUv2 is for hosts such as
Logic Pro; VST3 and CLAP need a host that supports those formats.

### Unsigned builds

Current candidates are not signed with a Developer ID or notarized. macOS may
block the application or prevent a DAW from scanning a downloaded plugin.

For the standalone application, try opening it, then use **System Settings >
Privacy & Security > Open Anyway** if macOS offers that option. Follow the
system's prompts to open this application.

If a plugin is blocked by download quarantine and you trust the package's source,
open Terminal and run only the command for each product you installed:

```sh
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Synth2.vst3"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/CLAP/Synth2.clap"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/Synth2.component"
```

These commands remove quarantine only from the specified Synth2 bundles. They
do not sign the products or disable Gatekeeper globally. Restart your DAW and
rescan afterward. Hosts or managed computers that require signed/notarized code
may still reject the build; do not disable system-wide protections to install it.

## Windows

Current Windows candidates are x64 builds. Extract the complete ZIP before
launching or installing products. In File Explorer, copy these products:

| Product in `Synth2/` | Destination folder |
| --- | --- |
| `Synth2.vst3` (entire folder) | `C:\Program Files\Common Files\VST3\` |
| `Synth2.clap` | `C:\Program Files\Common Files\CLAP\` |

These shared plugin folders usually need administrator permission. Create missing
folders as needed. Keep the complete VST3 bundle structure; do not copy only the
binary inside `Contents/x86_64-win/`. If your DAW supports a custom scan folder,
you can instead use a folder you own and configure it in the DAW's search paths.

For standalone use, keep `Synth2.exe` and any accompanying runtime files together
in a folder you own and launch the executable. You can create a shortcut; no DAW
is required.

### Unsigned builds

Current Windows candidates are not code-signed. SmartScreen may display a warning
when launching the standalone application. If you trust the downloaded package,
choose **More info > Run anyway** when that option is offered.

If Windows marks a downloaded file as blocked, right-click it, choose
**Properties**, and select **Unblock > Apply** if shown. You can also unblock the
downloaded ZIP before extracting it. Managed computers may enforce policies that
prevent unsigned software from running.

## Load and play

1. Restart the DAW after copying the plugins.
2. Enable the installed format, check its scan paths and run a plugin rescan.
   If Synth2 previously failed scanning, use the host's option to retry failed
   or blocked plugins.
3. Load **Synth2** on an instrument/MIDI track. Select the MIDI input, enable
   track monitoring and configure your audio output. In the standalone app,
   select the audio device and MIDI input in its settings.

Synth2 starts with Init and an empty library. Add your own `.sy1` patches or ZIP
banks through the library; no factory bank is included. The default audio quality
is 1x. Higher oversampling factors use more CPU and can change tone and level;
reduce the factor or increase your audio buffer if playback crackles.

If the plugin is missing, check its installation folder, the host's supported
formats and CPU architecture, and the DAW's scan/validation log. Run Apple Silicon DAWs natively; Intel/Rosetta hosts cannot load these arm64-only
plugins.
For AUv2 discovery problems, logging out and back in can refresh Audio Unit
registration before rescanning.

A successful build is not a guarantee of host/device compatibility. See the
[manual platform checks](https://github.com/yasuyuki-baba/synth2/blob/main/docs/validation/manual-platform.md) for verification scope.

## Uninstall

Close the DAW and standalone app, then remove the Synth2 products from their
installation folders. Rescan if the DAW still lists the plugin.

User library/settings are stored separately in `~/.local/share/Synth2/` on Linux
(or `$XDG_DATA_HOME/Synth2/` when configured),
`~/Library/Application Support/Synth2/` on macOS and `%APPDATA%\Synth2\` on
Windows. Removing the products leaves those files and your original imported
patches/banks in place. Keep your DAW projects and original patch files when
updating or uninstalling.
