SpoutBrowser adds [Spout](https://spout.zeal.co/) texture-sending support to a Chromium-based browser.  
This enables using any web content - WebGL, shaders, interactive pages - as a live video source in VJ software such as Resolume, MadMapper, and others.

**Demo video**: https://www.youtube.com/watch?v=vOFj6aN5QR4

<a href="https://www.youtube.com/watch?v=vOFj6aN5QR4">
 <img src="https://img.youtube.com/vi/vOFj6aN5QR4/maxresdefault.jpg" width="600" alt="Video cover"/>
</a>

# Implementation

This project is a fork of the Chromium Embedded Framework (CEF): https://github.com/chromiumembedded/cef  
It is based on the classic CEF sample application [cefclient](https://github.com/chromiumembedded/cef/tree/master/tests/cefclient).

Modifications to the CEF source are intentionally minimal to keep upgrading to newer CEF versions straightforward.  
This is the key difference from the existing [cef-spout](https://github.com/fg-uulm/cef-spout) project by Florian Geiselhart and the reason this project was created.

Spout integration uses CEF's off-screen rendering mode with shared textures and D3D11 rendering.  
See the [SpoutBrowser_TextureSender](tests/cefclient/spout_browser) module for details.


# Build

Prerequisites: Visual Studio 2022; CMake 3.21+; Python 3.9–3.11.  
Refer to the [CEF Project Setup](https://github.com/chromiumembedded/cef-project#setup) for details.

### Quick Start
1. Run `_SpoutBrowser_generate_solution.bat`. 
2. Open the generated solution: `/_cef_binary/[cef_distribution_name]/build/cef.sln`.
3. Build the **cefclient** project.

### Build Workflow
The build script automates the setup by working on top of prebuilt CEF binaries (similar to [cef-project](https://github.com/chromiumembedded/cef-project)):

1. **Download**: Fetches the specified CEF distribution into `/_cef_binary`.
2. **Patch**: Injects SpoutBrowser source code and modifies CMake scripts within the CEF directory.
3. **Generate**: Runs CMake to create the VS solution (this also fetches the Spout2 dependency).

The diagram below illustrates the sequence of operations performed during the build process.

<img src="_SpoutBrowser_build_diagram.svg" width="600" alt="Build diagram"/>


# Running

SpoutBrowser supports standard cefclient command-line switches (see [cefclient/README.md](tests/cefclient/README.md)).

**Common switches:**
* **--url="https://your-content.com"** - Sets the starting page.
* **--transparent-painting-enabled** - Enables the Alpha channel, useful for overlaying web content over other layers.
* **--off-screen-frame-rate=60** - Sets preferred FPS; default is 30.  
  Note: use fps 60 with **--multi-threaded-message-loop** to avoid dragging/resizing issues, related to [#4008](https://github.com/chromiumembedded/cef/issues/4008).
* **--always-on-top** - Keeps the browser window in the foreground.
* **--hide-controls** - Hides the address bar and navigation buttons.
* **--cache-path="cache"** - Specifies the directory where cache data will be stored.  
  Using different paths allows running multiple isolated instances.

Full list of switches: [common/client_switches.cc](tests/shared/common/client_switches.cc).

The following switches are ignored and forced to true: **--off-screen-rendering-enabled**, **--shared-texture-enabled**, **--use-alloy-style**.

**Pro Tip:** Create a .bat file for quick startup:
```batch
SpoutBrowser.exe --transparent-painting-enabled ^
                 --off-screen-frame-rate=60 --multi-threaded-message-loop ^
                 --url="https://threejs.org/examples/"
```

**Multiple Instances:** By default, cefclient reuses the main browser process.
This means if you launch multiple windows, subsequent ones will inherit some parameter values from the first process.
To run completely independent instances with their own settings, specify a unique **--cache-path** for each.

# Licenses

SpoutBrowser consists of several components with different licenses:

1. Chromium Embedded Framework (CEF)  
   Licensed under the BSD 3-Clause License.
   See LICENSE.txt for the original CEF license.

2. Spout (external dependency)  
   Spout is not included in this repository but is downloaded during CMake configuration.
   Spout is licensed under the BSD 2-Clause License:
   https://github.com/leadedge/Spout2

3. SpoutBrowser code  
   All additional code written specifically for SpoutBrowser is licensed under the MIT License.
   See LICENSE.spoutbrowser.txt for details.





