// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/renderer/client_renderer.h"

#include <sstream>
#include <string>

#include "include/cef_crash_util.h"
#include "include/cef_dom.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"

namespace client::renderer {

namespace {

// Must match the value in client_handler.cc.
const char kFocusedNodeChangedMessage[] = "ClientRenderer.FocusedNodeChanged";

class ClientRenderDelegate : public ClientAppRenderer::Delegate {
 public:
  ClientRenderDelegate() = default;

  void OnWebKitInitialized(CefRefPtr<ClientAppRenderer> app) override {
    if (CefCrashReportingEnabled()) {
      // Set some crash keys for testing purposes. Keys must be defined in the
      // "crash_reporter.cfg" file. See cef_crash_util.h for details.
      CefSetCrashKeyValue("testkey_small1", "value1_small_renderer");
      CefSetCrashKeyValue("testkey_small2", "value2_small_renderer");
      CefSetCrashKeyValue("testkey_medium1", "value1_medium_renderer");
      CefSetCrashKeyValue("testkey_medium2", "value2_medium_renderer");
      CefSetCrashKeyValue("testkey_large1", "value1_large_renderer");
      CefSetCrashKeyValue("testkey_large2", "value2_large_renderer");
    }

    // Create the renderer-side router for query handling.
    CefMessageRouterConfig config;
    message_router_ = CefMessageRouterRendererSide::Create(config);
  }

  void OnContextCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override {
    message_router_->OnContextCreated(browser, frame, context);

    // Inject the device picker JavaScript shim into main frames only.
    if (frame->IsMain()) {
      const std::string kDevicePickerJS = R"JS(
(function() {
  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) return;
  if (window.__spoutDevicePickerInstalled) return;
  window.__spoutDevicePickerInstalled = true;

  const _origGetUserMedia = navigator.mediaDevices.getUserMedia.bind(
      navigator.mediaDevices);

  function getStoredDeviceId(kind) {
    try {
      return sessionStorage.getItem('__spout_' + kind + '_deviceId');
    } catch(e) { return null; }
  }
  function storeDeviceId(kind, id) {
    try {
      sessionStorage.setItem('__spout_' + kind + '_deviceId', id);
    } catch(e) {}
  }

  function createPickerUI(devices, onConfirm) {
    const cameras = devices.filter(d => d.kind === 'videoinput');
    const mics = devices.filter(d => d.kind === 'audioinput');

    const overlay = document.createElement('div');
    overlay.id = '__spout_device_picker_overlay';
    overlay.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;'
      + 'background:rgba(0,0,0,0.7);z-index:2147483647;display:flex;'
      + 'align-items:center;justify-content:center;font-family:system-ui,sans-serif;';

    const card = document.createElement('div');
    card.style.cssText = 'background:#1e1e2e;border-radius:12px;padding:28px 32px;'
      + 'min-width:340px;max-width:480px;color:#cdd6f4;box-shadow:0 8px 32px rgba(0,0,0,0.5);';

    let html = '<h2 style="margin:0 0 20px;font-size:18px;color:#89b4fa;">'
      + '&#127909; SpoutBrowser Device Selector</h2>';

    if (cameras.length > 0) {
      html += '<label style="display:block;margin-bottom:6px;font-size:13px;'
        + 'color:#a6adc8;">Camera</label>';
      html += '<select id="__spout_camera_select" style="width:100%;padding:8px 12px;'
        + 'border-radius:6px;border:1px solid #45475a;background:#313244;color:#cdd6f4;'
        + 'font-size:14px;margin-bottom:16px;outline:none;cursor:pointer;">';
      cameras.forEach(function(d, i) {
        html += '<option value="' + d.deviceId + '">'
          + (d.label || 'Camera ' + (i+1)) + '</option>';
      });
      html += '</select>';
    }

    if (mics.length > 0) {
      html += '<label style="display:block;margin-bottom:6px;font-size:13px;'
        + 'color:#a6adc8;">Microphone</label>';
      html += '<select id="__spout_mic_select" style="width:100%;padding:8px 12px;'
        + 'border-radius:6px;border:1px solid #45475a;background:#313244;color:#cdd6f4;'
        + 'font-size:14px;margin-bottom:20px;outline:none;cursor:pointer;">';
      mics.forEach(function(d, i) {
        html += '<option value="' + d.deviceId + '">'
          + (d.label || 'Microphone ' + (i+1)) + '</option>';
      });
      html += '</select>';
    }

    html += '<button id="__spout_confirm_btn" style="width:100%;padding:10px;'
      + 'border-radius:6px;border:none;background:#89b4fa;color:#1e1e2e;'
      + 'font-size:14px;font-weight:600;cursor:pointer;transition:background 0.2s;">'
      + 'Confirm</button>';

    card.innerHTML = html;
    overlay.appendChild(card);
    document.body.appendChild(overlay);

    const btn = document.getElementById('__spout_confirm_btn');
    btn.addEventListener('mouseover', function() {
      btn.style.background = '#74c7ec';
    });
    btn.addEventListener('mouseout', function() {
      btn.style.background = '#89b4fa';
    });
    btn.addEventListener('click', function() {
      const camSelect = document.getElementById('__spout_camera_select');
      const micSelect = document.getElementById('__spout_mic_select');
      const camId = camSelect ? camSelect.value : null;
      const micId = micSelect ? micSelect.value : null;
      overlay.remove();
      onConfirm(camId, micId);
    });
  }

  navigator.mediaDevices.getUserMedia = function(constraints) {
    if (!constraints) return _origGetUserMedia(constraints);

    const needsVideo = !!constraints.video;
    const needsAudio = !!constraints.audio;

    const storedCam = getStoredDeviceId('videoinput');
    const storedMic = getStoredDeviceId('audioinput');

    const hasStoredCam = storedCam && storedCam !== "";
    const hasStoredMic = storedMic && storedMic !== "";

    // If we already have stored selections, apply them directly.
    if ((!needsVideo || hasStoredCam) && (!needsAudio || hasStoredMic)) {
      if (needsVideo && hasStoredCam) {
        if (typeof constraints.video === 'object') {
          constraints.video.deviceId = { exact: storedCam };
        } else {
          constraints.video = { deviceId: { exact: storedCam } };
        }
      }
      if (needsAudio && hasStoredMic) {
        if (typeof constraints.audio === 'object') {
          constraints.audio.deviceId = { exact: storedMic };
        } else {
          constraints.audio = { deviceId: { exact: storedMic } };
        }
      }
      return _origGetUserMedia(constraints);
    }

    function showPickerOrProceed(devices) {
      const cameras = devices.filter(d => d.kind === 'videoinput' && d.deviceId && d.deviceId !== "");
      const mics = devices.filter(d => d.kind === 'audioinput' && d.deviceId && d.deviceId !== "");

      const multipleCams = needsVideo && cameras.length > 1 && !hasStoredCam;
      const multipleMics = needsAudio && mics.length > 1 && !hasStoredMic;

      if (!multipleCams && !multipleMics) {
        // Only one device or already selected, proceed normally.
        if (needsVideo && cameras.length === 1) {
          storeDeviceId('videoinput', cameras[0].deviceId);
        }
        if (needsAudio && mics.length === 1) {
          storeDeviceId('audioinput', mics[0].deviceId);
        }
        return _origGetUserMedia(constraints);
      }

      // Show the picker UI.
      return new Promise(function(resolve, reject) {
        function waitForBody() {
          if (document.body) {
            createPickerUI(devices, function(camId, micId) {
              if (camId && camId !== "") {
                storeDeviceId('videoinput', camId);
                if (typeof constraints.video === 'object') {
                  constraints.video.deviceId = { exact: camId };
                } else {
                  constraints.video = { deviceId: { exact: camId } };
                }
              }
              if (micId && micId !== "") {
                storeDeviceId('audioinput', micId);
                if (typeof constraints.audio === 'object') {
                  constraints.audio.deviceId = { exact: micId };
                } else {
                  constraints.audio = { deviceId: { exact: micId } };
                }
              }
              _origGetUserMedia(constraints).then(resolve).catch(reject);
            });
          } else {
            setTimeout(waitForBody, 50);
          }
        }
        waitForBody();
      });
    }

    // Enumerate devices and check if we have access to device labels/IDs.
    return navigator.mediaDevices.enumerateDevices().then(function(devices) {
      const hasLabels = devices.some(d => d.label !== "");
      
      // If we don't have labels yet, we need to bootstrap permission by making a quick temporary request.
      if (!hasLabels && (needsVideo || needsAudio)) {
        return _origGetUserMedia({ video: needsVideo, audio: needsAudio }).then(function(tempStream) {
          // Instantly stop the temporary stream's tracks so we don't hold the camera active.
          tempStream.getTracks().forEach(track => track.stop());
          // Query the devices list again now that permission is granted.
          return navigator.mediaDevices.enumerateDevices();
        }).then(function(labeledDevices) {
          return showPickerOrProceed(labeledDevices);
        }).catch(function() {
          // Fallback to default in case of any failure.
          return _origGetUserMedia(constraints);
        });
      }

      return showPickerOrProceed(devices);
    });
  };
})();
)JS";

      frame->ExecuteJavaScript(kDevicePickerJS, frame->GetURL(), 0);
    }
  }

  void OnContextReleased(CefRefPtr<ClientAppRenderer> app,
                         CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override {
    message_router_->OnContextReleased(browser, frame, context);
  }

  void OnFocusedNodeChanged(CefRefPtr<ClientAppRenderer> app,
                            CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefDOMNode> node) override {
    bool is_editable = (node.get() && node->IsEditable());
    if (is_editable != last_node_is_editable_) {
      // Notify the browser of the change in focused element type.
      last_node_is_editable_ = is_editable;
      CefRefPtr<CefProcessMessage> message =
          CefProcessMessage::Create(kFocusedNodeChangedMessage);
      message->GetArgumentList()->SetBool(0, is_editable);
      frame->SendProcessMessage(PID_BROWSER, message);
    }
  }

  bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    return message_router_->OnProcessMessageReceived(browser, frame,
                                                     source_process, message);
  }

 private:
  bool last_node_is_editable_ = false;

  // Handles the renderer side of query routing.
  CefRefPtr<CefMessageRouterRendererSide> message_router_;

  DISALLOW_COPY_AND_ASSIGN(ClientRenderDelegate);
  IMPLEMENT_REFCOUNTING(ClientRenderDelegate);
};

}  // namespace

void CreateDelegates(ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new ClientRenderDelegate);
}

}  // namespace client::renderer
