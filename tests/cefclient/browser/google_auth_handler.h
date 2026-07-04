#ifndef CEF_TESTS_CEFCLIENT_BROWSER_GOOGLE_AUTH_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_GOOGLE_AUTH_HANDLER_H_
#pragma once

#include "include/base/cef_callback.h"
#include "include/cef_server.h"
#include "include/cef_urlrequest.h"
#include "include/wrapper/cef_message_router.h"

namespace client {

class GoogleAuthHandler : public CefMessageRouterBrowserSide::Handler,
                          public CefServerHandler {
 public:
  GoogleAuthHandler();
  ~GoogleAuthHandler() override;

  // CefMessageRouterBrowserSide::Handler methods
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override;

  // CefServerHandler methods (called on the dedicated server thread)
  void OnServerCreated(CefRefPtr<CefServer> server) override;
  void OnServerDestroyed(CefRefPtr<CefServer> server) override;
  void OnClientConnected(CefRefPtr<CefServer> server, int connection_id) override {}
  void OnClientDisconnected(CefRefPtr<CefServer> server, int connection_id) override {}
  void OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override;
  void OnWebSocketRequest(CefRefPtr<CefServer> server,
                          int connection_id,
                          const CefString& client_address,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefCallback> callback) override {
    callback->Cancel();
  }
  void OnWebSocketConnected(CefRefPtr<CefServer> server, int connection_id) override {}
  void OnWebSocketMessage(CefRefPtr<CefServer> server,
                          int connection_id,
                          const void* data,
                          size_t data_size) override {}

 private:
  void StartLocalServer();
  void StopLocalServer();
  void LaunchSystemBrowser();
  void ExchangeCodeForTokens(const std::string& code);
  void RefreshAccessToken(const std::string& refresh_token);
  void FetchUserProfile(const std::string& access_token);
  
  bool LoadSession();
  bool SaveSession(const std::string& access_token,
                   const std::string& refresh_token,
                   const std::string& email,
                   const std::string& name);
  void ClearSession();
  void SendSuccessToJS(const std::string& response);
  void SendFailureToJS(const std::string& error_msg);

  // Callback wrapper for URL requests
  class RequestClient : public CefURLRequestClient {
   public:
    using RequestCallback = base::OnceCallback<void(int status_code, const std::string& data)>;
    explicit RequestClient(RequestCallback callback);
    void OnRequestComplete(CefRefPtr<CefURLRequest> request) override;
    void OnUploadProgress(CefRefPtr<CefURLRequest> request, int64_t current, int64_t total) override {}
    void OnDownloadProgress(CefRefPtr<CefURLRequest> request, int64_t current, int64_t total) override {}
    void OnDownloadData(CefRefPtr<CefURLRequest> request, const void* data, size_t data_length) override;
    bool GetAuthCredentials(bool isProxy,
                            const CefString& host,
                            int port,
                            const CefString& realm,
                            const CefString& scheme,
                            CefRefPtr<CefAuthCallback> callback) override { return false; }
   private:
    RequestCallback callback_;
    std::string download_data_;
    IMPLEMENT_REFCOUNTING(RequestClient);
    DISALLOW_COPY_AND_ASSIGN(RequestClient);
  };

  CefRefPtr<CefServer> server_;
  CefRefPtr<Callback> js_callback_;

  std::string code_verifier_;
  std::string code_challenge_;
  std::string state_;

  // Stored profile info
  std::string cached_user_info_;

  IMPLEMENT_REFCOUNTING(GoogleAuthHandler);
  DISALLOW_COPY_AND_ASSIGN(GoogleAuthHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_GOOGLE_AUTH_HANDLER_H_
