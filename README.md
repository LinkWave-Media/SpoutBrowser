SpoutBrowser adds [Spout](https://spout.zeal.co/) texture-sending support to a Chromium-based browser.  
This enables using any web content - WebGL, shaders, interactive pages - as a live video source in VJ software such as Resolume, MadMapper, and others.


# Implementation

This project is a fork of the Chromium Embedded Framework (CEF): https://github.com/chromiumembedded/cef  
It is based on the classic CEF sample application [cefclient](https://github.com/chromiumembedded/cef/tree/master/tests/cefclient).

Modifications to the CEF source are intentionally minimal to keep upgrading to newer CEF versions straightforward.  
(This is the key difference from the existing [cef-spout](https://github.com/fg-uulm/cef-spout) project by Florian Geiselhart and the reason this project was created.)

Spout integration uses CEF's off-screen rendering mode with shared textures and D3D11 rendering.  
See the [SpoutBrowser_TextureSender](https://github.com/bntre/SpoutBrowser/tree/spout/tests/cefclient/spout_browser) module for details.


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
