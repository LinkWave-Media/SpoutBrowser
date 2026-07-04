#include "tests/cefclient/browser/google_auth_handler.h"

#include <windows.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <shellapi.h>
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/main_context.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace client {

namespace {

// IMPORTANT: Replace this placeholder client ID with your actual Google OAuth Client ID
// (configured as a "Desktop app" client type in the Google Cloud Console).
const char kGoogleClientId[] = "467270992671-fdemtfgad80mok466hv1tb0lpgrr5ien.apps.googleusercontent.com";

const char kGoogleAuthUrl[] = "https://accounts.google.com/o/oauth2/v2/auth";
const char kGoogleTokenUrl[] = "https://oauth2.googleapis.com/token";
const char kGoogleUserInfoUrl[] = "https://www.googleapis.com/oauth2/v3/userinfo";
const char kLocalhostAddress[] = "127.0.0.1";
const int kLocalhostPort = 3000;
const int kServerBacklog = 5;

// URL encoding helper
std::string UrlEncode(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;
  for (char c : value) {
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else {
      escaped << '%' << std::setw(2) << int((unsigned char)c);
    }
  }
  return escaped.str();
}

// Generate secure random string for PKCE and State
std::string GenerateRandomString(size_t length) {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  std::string result;
  result.resize(length);
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 2);
  for (size_t i = 0; i < length; ++i) {
    result[i] = charset[distribution(generator)];
  }
  return result;
}

// SHA256 hashing using Windows BCrypt API
std::vector<uint8_t> CalculateSha256(const std::string& input) {
  BCRYPT_ALG_HANDLE hAlg = NULL;
  BCRYPT_HASH_HANDLE hHash = NULL;
  DWORD cbHashObject = 0, cbHash = 0, cbData = 0;
  std::vector<uint8_t> hash;

  if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0))) {
    if (BCRYPT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0))) {
      std::vector<uint8_t> hashObject(cbHashObject);
      if (BCRYPT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0))) {
        hash.resize(cbHash);
        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, hashObject.data(), cbHashObject, NULL, 0, 0))) {
          if (BCRYPT_SUCCESS(BCryptHashData(hHash, (PBYTE)input.c_str(), (ULONG)input.length(), 0))) {
            BCryptFinishHash(hHash, hash.data(), cbHash, 0);
          }
          BCryptDestroyHash(hHash);
        }
      }
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
  }
  return hash;
}

// Base64Url encoding
std::string Base64UrlEncode(const std::vector<uint8_t>& data) {
  CefString encoded = CefBase64Encode(data.data(), data.size());
  std::string s = encoded.ToString();
  std::replace(s.begin(), s.end(), '+', '-');
  std::replace(s.begin(), s.end(), '/', '_');
  s.erase(std::remove(s.begin(), s.end(), '='), s.end());
  return s;
}

// Get file path for stored session
std::string GetSessionFilePath() {
  return MainContext::Get()->GetAppWorkingDirectory() + "cache\\session.dat";
}

// DPAPI Encryption helper
bool EncryptData(const std::string& input, std::vector<uint8_t>& output) {
  DATA_BLOB data_in;
  data_in.pbData = (BYTE*)input.c_str();
  data_in.cbData = (DWORD)input.length();

  DATA_BLOB data_out;
  if (CryptProtectData(&data_in, L"SpoutBrowser Session", NULL, NULL, NULL, 0, &data_out)) {
    output.assign(data_out.pbData, data_out.pbData + data_out.cbData);
    LocalFree(data_out.pbData);
    return true;
  }
  return false;
}

// DPAPI Decryption helper
bool DecryptData(const std::vector<uint8_t>& input, std::string& output) {
  DATA_BLOB data_in;
  data_in.pbData = (BYTE*)input.data();
  data_in.cbData = (DWORD)input.size();

  DATA_BLOB data_out;
  if (CryptUnprotectData(&data_in, NULL, NULL, NULL, NULL, 0, &data_out)) {
    output.assign((char*)data_out.pbData, data_out.cbData);
    LocalFree(data_out.pbData);
    return true;
  }
  return false;
}

}  // namespace

GoogleAuthHandler::GoogleAuthHandler() = default;

GoogleAuthHandler::~GoogleAuthHandler() {
  StopLocalServer();
}

bool GoogleAuthHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int64_t query_id,
                               const CefString& request,
                               bool persistent,
                               CefRefPtr<Callback> callback) {
  CEF_REQUIRE_UI_THREAD();

  const std::string& req = request.ToString();

  if (req == "google-check-auth") {
    if (LoadSession()) {
      callback->Success(cached_user_info_);
    } else {
      callback->Success("none");
    }
    return true;
  }

  if (req == "google-logout") {
    ClearSession();
    callback->Success("success");
    return true;
  }

  if (req == "google-login") {
    if (js_callback_) {
      js_callback_->Failure(0, "A login query is already in progress.");
    }
    js_callback_ = callback;
    
    // Generate PKCE values
    code_verifier_ = GenerateRandomString(64);
    std::vector<uint8_t> hash = CalculateSha256(code_verifier_);
    code_challenge_ = Base64UrlEncode(hash);
    state_ = GenerateRandomString(16);

    StartLocalServer();
    return true;
  }

  return false;
}

void GoogleAuthHandler::OnServerCreated(CefRefPtr<CefServer> server) {
  server_ = server;
  if (server->IsRunning()) {
    LaunchSystemBrowser();
  } else {
    SendFailureToJS("Local authentication server failed to start.");
  }
}

void GoogleAuthHandler::OnServerDestroyed(CefRefPtr<CefServer> server) {
  server_ = nullptr;
}

void GoogleAuthHandler::OnHttpRequest(CefRefPtr<CefServer> server,
                                     int connection_id,
                                     const CefString& client_address,
                                     CefRefPtr<CefRequest> request) {
  CefURLParts url_parts;
  CefParseURL(request->GetURL(), url_parts);
  std::string path = CefString(&url_parts.path).ToString();

  if (path == "/callback") {
    // Parse query params
    std::string query = CefString(&url_parts.query).ToString();
    std::string code;
    std::string received_state;

    std::stringstream ss(query);
    std::string item;
    while (std::getline(ss, item, '&')) {
      size_t eq = item.find('=');
      if (eq != std::string::npos) {
        std::string key = item.substr(0, eq);
        std::string val = item.substr(eq + 1);
        if (key == "code") {
          code = val;
        } else if (key == "state") {
          received_state = val;
        }
      }
    }

    if (received_state != state_) {
      server->SendHttp500Response(connection_id, "State parameter mismatch (potential CSRF).");
      SendFailureToJS("State validation failed.");
      StopLocalServer();
      return;
    }

    if (code.empty()) {
      server->SendHttp500Response(connection_id, "Authorization code was not returned.");
      SendFailureToJS("Authorization code missing.");
      StopLocalServer();
      return;
    }

    // Send successful webpage back to the system browser
    std::string html = 
      "<html><head><style>"
      "body { font-family: system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; text-align: center; padding: 50px; }"
      "h1 { color: #a6e3a1; }"
      "p { color: #a6adc8; }"
      "</style></head><body>"
      "<h1>Login Successful!</h1>"
      "<p>Authentication completed. You can safely close this browser window/tab.</p>"
      "</body></html>";
    
    server->SendHttpResponse(connection_id, 200, "text/html", html.length(), CefServer::HeaderMap());
    server->SendRawData(connection_id, html.c_str(), html.length());
    server->CloseConnection(connection_id);

    // Switch back to UI thread to process token exchange
    CefPostTask(TID_UI, base::BindOnce(&GoogleAuthHandler::ExchangeCodeForTokens, this, code));
    
    // Shut down server
    StopLocalServer();
  } else {
    server->SendHttp404Response(connection_id);
  }
}

void GoogleAuthHandler::StartLocalServer() {
  CefServer::CreateServer(kLocalhostAddress, kLocalhostPort, kServerBacklog, this);
}

void GoogleAuthHandler::StopLocalServer() {
  if (server_) {
    server_->Shutdown();
  }
}

void GoogleAuthHandler::LaunchSystemBrowser() {
  std::string auth_url = std::string(kGoogleAuthUrl) +
      "?client_id=" + UrlEncode(kGoogleClientId) +
      "&redirect_uri=" + UrlEncode("http://localhost:3000/callback") +
      "&response_type=code" +
      "&scope=" + UrlEncode("openid email profile") +
      "&access_type=offline" +
      "&code_challenge=" + UrlEncode(code_challenge_) +
      "&code_challenge_method=S256" +
      "&state=" + UrlEncode(state_);

  // Launch system default web browser
  ShellExecuteA(NULL, "open", auth_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void GoogleAuthHandler::ExchangeCodeForTokens(const std::string& code) {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefRequest> request = CefRequest::Create();
  request->SetURL(kGoogleTokenUrl);
  request->SetMethod("POST");

  std::string post_data = 
      "client_id=" + UrlEncode(kGoogleClientId) +
      "&code=" + UrlEncode(code) +
      "&code_verifier=" + UrlEncode(code_verifier_) +
      "&redirect_uri=" + UrlEncode("http://localhost:3000/callback") +
      "&grant_type=authorization_code";

  CefRefPtr<CefPostData> postData = CefPostData::Create();
  CefRefPtr<CefPostDataElement> element = CefPostDataElement::Create();
  element->SetToBytes(post_data.length(), post_data.c_str());
  postData->AddElement(element);
  request->SetPostData(postData);

  CefRequest::HeaderMap headers;
  headers.insert(std::make_pair("Content-Type", "application/x-www-form-urlencoded"));
  request->SetHeaderMap(headers);

  CefRefPtr<RequestClient> client = new RequestClient(base::BindOnce(
      [](CefRefPtr<GoogleAuthHandler> handler, int status, const std::string& data) {
        if (status == 200) {
          CefRefPtr<CefValue> parsed = CefParseJSON(data, JSON_PARSER_ALLOW_TRAILING_COMMAS);
          if (parsed && parsed->GetType() == VTYPE_DICTIONARY) {
            CefRefPtr<CefDictionaryValue> dict = parsed->GetDictionary();
            std::string access_token = dict->GetString("access_token").ToString();
            std::string refresh_token = dict->GetString("refresh_token").ToString();
            
            // Fetch profile info next
            handler->FetchUserProfile(access_token);
            
            // Persist tokens
            handler->SaveSession(access_token, refresh_token, "", "");
            return;
          }
        }
        handler->SendFailureToJS("Token exchange failed: HTTP " + std::to_string(status));
      }, CefRefPtr<GoogleAuthHandler>(this)));

  CefURLRequest::Create(request, client.get(), nullptr);
}

void GoogleAuthHandler::FetchUserProfile(const std::string& access_token) {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefRequest> request = CefRequest::Create();
  request->SetURL(kGoogleUserInfoUrl);
  request->SetMethod("GET");

  CefRequest::HeaderMap headers;
  headers.insert(std::make_pair("Authorization", "Bearer " + access_token));
  request->SetHeaderMap(headers);

  CefRefPtr<RequestClient> client = new RequestClient(base::BindOnce(
      [](CefRefPtr<GoogleAuthHandler> handler, int status, const std::string& data) {
        if (status == 200) {
          CefRefPtr<CefValue> parsed = CefParseJSON(data, JSON_PARSER_ALLOW_TRAILING_COMMAS);
          if (parsed && parsed->GetType() == VTYPE_DICTIONARY) {
            CefRefPtr<CefDictionaryValue> dict = parsed->GetDictionary();
            std::string email = dict->GetString("email").ToString();
            std::string name = dict->GetString("name").ToString();

            // Load stored tokens to keep refresh_token
            std::string old_session;
            std::string access_token;
            std::string refresh_token;
            
            std::string file_path = GetSessionFilePath();
            FILE* f = fopen(file_path.c_str(), "rb");
            if (f) {
              fseek(f, 0, SEEK_END);
              long size = ftell(f);
              fseek(f, 0, SEEK_SET);
              std::vector<uint8_t> buffer(size);
              fread(buffer.data(), 1, size, f);
              fclose(f);
              DecryptData(buffer, old_session);
              CefRefPtr<CefValue> parsed_old = CefParseJSON(old_session, JSON_PARSER_ALLOW_TRAILING_COMMAS);
              if (parsed_old && parsed_old->GetType() == VTYPE_DICTIONARY) {
                access_token = parsed_old->GetDictionary()->GetString("access_token").ToString();
                refresh_token = parsed_old->GetDictionary()->GetString("refresh_token").ToString();
              }
            }

            handler->SaveSession(access_token, refresh_token, email, name);
            handler->SendSuccessToJS(handler->cached_user_info_);
            return;
          }
        }
        handler->SendFailureToJS("Failed fetching profile info: HTTP " + std::to_string(status));
      }, CefRefPtr<GoogleAuthHandler>(this)));

  CefURLRequest::Create(request, client.get(), nullptr);
}

bool GoogleAuthHandler::LoadSession() {
  std::string file_path = GetSessionFilePath();
  FILE* f = fopen(file_path.c_str(), "rb");
  if (!f) return false;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> buffer(size);
  fread(buffer.data(), 1, size, f);
  fclose(f);

  std::string decrypted;
  if (DecryptData(buffer, decrypted)) {
    cached_user_info_ = decrypted;
    return true;
  }
  return false;
}

bool GoogleAuthHandler::SaveSession(const std::string& access_token,
                                    const std::string& refresh_token,
                                    const std::string& email,
                                    const std::string& name) {
  CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
  dict->SetString("access_token", access_token);
  dict->SetString("refresh_token", refresh_token);
  dict->SetString("email", email);
  dict->SetString("name", name);
  
  CefRefPtr<CefValue> val = CefValue::Create();
  val->SetDictionary(dict);
  std::string raw_json = CefWriteJSON(val, JSON_WRITER_DEFAULT).ToString();

  std::vector<uint8_t> encrypted;
  if (EncryptData(raw_json, encrypted)) {
    std::string file_path = GetSessionFilePath();
    FILE* f = fopen(file_path.c_str(), "wb");
    if (f) {
      fwrite(encrypted.data(), 1, encrypted.size(), f);
      fclose(f);
      cached_user_info_ = raw_json;
      return true;
    }
  }
  return false;
}

void GoogleAuthHandler::ClearSession() {
  std::string file_path = GetSessionFilePath();
  remove(file_path.c_str());
  cached_user_info_.clear();
}

void GoogleAuthHandler::SendSuccessToJS(const std::string& response) {
  if (js_callback_) {
    js_callback_->Success(response);
    js_callback_ = nullptr;
  }
}

void GoogleAuthHandler::SendFailureToJS(const std::string& error_msg) {
  if (js_callback_) {
    js_callback_->Failure(500, error_msg);
    js_callback_ = nullptr;
  }
}

// RequestClient implementation
GoogleAuthHandler::RequestClient::RequestClient(
    GoogleAuthHandler::RequestClient::RequestCallback callback)
    : callback_(std::move(callback)) {}

void GoogleAuthHandler::RequestClient::OnRequestComplete(CefRefPtr<CefURLRequest> request) {
  CEF_REQUIRE_UI_THREAD();
  int status = 0;
  if (request->GetRequestStatus() == UR_SUCCESS) {
    status = request->GetResponse()->GetStatus();
  }
  if (!callback_.is_null()) {
    std::move(callback_).Run(status, download_data_);
  }
}

void GoogleAuthHandler::RequestClient::OnDownloadData(CefRefPtr<CefURLRequest> request,
                                                      const void* data,
                                                      size_t data_length) {
  CEF_REQUIRE_UI_THREAD();
  download_data_ += std::string(static_cast<const char*>(data), data_length);
}

}  // namespace client
