#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================== USER CONFIG ========================
static const char* AP_SSID = "DyAuto_Config";
// IMPORTANT: Set a unique password (8+ characters) before deployment.
static const char* AP_PASSWORD = "12345678";

// For production, use a private broker with per-device credentials and TLS.
// This sample uses plain TCP (1883); do not treat topic names as authentication.
static const char* MQTT_HOST = "broker.emqx.io";
static constexpr uint16_t MQTT_PORT = 1883;
static const char* MQTT_USERNAME = ""; // Set broker username
static const char* MQTT_PASSWORD = ""; // Set broker password
static const char* GATEWAY_ID = "GW_01";
static const char* DEVICE_SECRET = "143"; // Must match the QR/app topic during this connectivity test.

static constexpr uint8_t BUTTON_PIN = 0; // BOOT, active LOW
static constexpr uint8_t LED_PIN = 2;    // Board-dependent; verify for your ESP32
static constexpr uint16_t DNS_PORT = 53;
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
static constexpr unsigned long PORTAL_SUCCESS_GRACE_MS = 120000; // Keep success page available for 2 minutes
static constexpr unsigned long WIFI_SCAN_TIMEOUT_MS = 20000;
static constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5000;
static constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 5000;
static constexpr unsigned long STATUS_INTERVAL_MS = 30000;
static constexpr unsigned long BUTTON_DEBOUNCE_MS = 40;
static constexpr unsigned long BUTTON_HOLD_MS = 5000;

DNSServer dnsServer;
WebServer server(80);
Preferences preferences;
WiFiClient networkClient;
PubSubClient mqttClient(networkClient);

String statusTopic;
String commandTopic;
String pendingSSID;
String pendingPassword;
String connectionMessage = "Đang kết nối Wi-Fi...";

enum class AppMode : uint8_t { AP_CONFIG, STA_CONNECTING, STA_ONLINE };
enum class PortalResult : uint8_t { IDLE, CONNECTING, SUCCESS, FAILED };
enum class ScanState : uint8_t { IDLE, RUNNING, DONE, ERROR };
AppMode appMode = AppMode::AP_CONFIG;
PortalResult portalResult = PortalResult::IDLE;
ScanState scanState = ScanState::IDLE;

bool portalActive = false;
bool portalServerConfigured = false;
bool portalConnectAttempt = false;
bool saveCredentialsOnConnect = false;
bool currentLedState = false; // TEST ONLY: LED state simulates Node 1; not a physical load state.
String lastCommandId = "";
String lastCommandResult = "boot";
unsigned long lastCommandAt = 0;
static constexpr unsigned long COMMAND_COOLDOWN_MS = 500;
unsigned long wifiAttemptStartedAt = 0;
unsigned long portalCloseAt = 0;
unsigned long scanStartedAt = 0;
unsigned long lastMqttAttemptAt = 0;
unsigned long lastStatusPublishAt = 0;
unsigned long lastWiFiRetryAt = 0;

int rawButtonLast = HIGH;
int stableButtonState = HIGH;
unsigned long lastButtonChangeAt = 0;
unsigned long buttonDownAt = 0;
bool longPressHandled = false;

// ======================== WEB UI =============================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="vi"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light">
<title>AquaAuto - Cấp mạng Gateway</title>
<style>
*{box-sizing:border-box}body{font-family:system-ui,-apple-system,sans-serif;background:#f1f5f9;padding:16px;margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;color:#334155}
.card{background:#fff;padding:24px;border-radius:16px;box-shadow:0 4px 16px #0001;width:100%;max-width:440px}.header{text-align:center;border-bottom:1px solid #e2e8f0;padding-bottom:16px;margin-bottom:20px}
h2{color:#047857;margin:0 0 6px;font-size:22px}.sub{font-size:13px;color:#64748b;margin:0}label{font-weight:650;font-size:14px;display:block;margin:14px 0 8px}
select,input{width:100%;padding:12px;border:1px solid #cbd5e1;border-radius:8px;font-size:16px;background:#f8fafc}.group{position:relative}.toggle{position:absolute;right:8px;top:7px;border:0;background:transparent;padding:6px;cursor:pointer}
.submit,.secondary{width:100%;background:#059669;color:#fff;border:0;padding:14px;font-size:16px;font-weight:700;border-radius:8px;cursor:pointer;margin-top:18px}.secondary{background:#e2e8f0;color:#334155;margin-top:10px}.submit:disabled{opacity:.6;cursor:wait}
#msg{display:none;padding:12px;border-radius:8px;margin:14px 0;text-align:left;font-size:14px;white-space:pre-wrap;line-height:1.5}.error{display:block!important;background:#fee2e2;color:#991b1b}.success{display:block!important;background:#d1fae5;color:#065f46}.info{display:block!important;background:#e0f2fe;color:#075985}
#loading{text-align:center;color:#059669;margin:20px 0}.hint{font-size:13px;color:#64748b;line-height:1.5;margin-top:12px}.hidden{display:none!important}
</style></head><body><main class="card"><div class="header"><h2>📡 Cài đặt mạng AquaAuto</h2><p class="sub">Chỉ cần thực hiện một lần cho Gateway này</p></div>
<div id="loading">🔄 Đang tìm Wi-Fi quanh đây…</div><div id="msg" role="status" aria-live="polite"></div>
<form id="wifi-form" class="hidden"><label for="ssid-list">1. Chọn Wi-Fi tại nhà/vườn</label><select name="ssid" id="ssid-list" required></select>
<label for="pass-input">2. Mật khẩu Wi-Fi</label><div class="group"><input type="password" name="pass" id="pass-input" placeholder="Nhập mật khẩu Wi-Fi" autocomplete="current-password" maxlength="63" required>
<button type="button" class="toggle" id="toggle-btn" aria-label="Hiện hoặc ẩn mật khẩu">👁️</button></div>
<button type="submit" id="submit-btn" class="submit">Lưu và kết nối</button><button type="button" id="rescan-btn" class="secondary">Quét Wi-Fi lại</button></form>
<div id="success-actions" class="hidden"><p class="hint">Bây giờ hãy đóng cửa sổ cấu hình này, mở Camera hoặc Zalo và quét mã QR AquaAuto trên tủ điện để mở ứng dụng điều khiển. Không cần cấu hình lại trong những lần sử dụng sau.</p></div>
<p class="hint" id="fallback-hint">Nếu trang này không tự mở, hãy mở trình duyệt và nhập <b>http://192.168.4.1</b>.</p></main>
<script>
const $=id=>document.getElementById(id);let scanPoll=null;let connectPoll=null;
function showMsg(message,type='info'){$('msg').textContent=message;$('msg').className=type;$('msg').style.display='';}
function stopPolls(){if(scanPoll){clearInterval(scanPoll);scanPoll=null;}if(connectPoll){clearInterval(connectPoll);connectPoll=null;}}
$('toggle-btn').addEventListener('click',()=>{const p=$('pass-input');p.type=p.type==='password'?'text':'password';$('toggle-btn').textContent=p.type==='password'?'👁️':'🙈';});
async function loadNetworks(){stopPolls();$('loading').classList.remove('hidden');$('loading').textContent='🔄 Đang tìm Wi-Fi quanh đây…';$('wifi-form').classList.add('hidden');$('success-actions').classList.add('hidden');showMsg('Đang quét các mạng Wi-Fi gần Gateway.','info');try{let r=await fetch('/scan/start',{cache:'no-store'});if(!r.ok)throw Error('Không thể bắt đầu quét Wi-Fi');let d=await r.json();if(d.status==='done'){await showNetworks();return;}scanPoll=setInterval(async()=>{try{const sr=await fetch('/scan/status',{cache:'no-store'});const st=await sr.json();if(st.status==='done'){stopPolls();await showNetworks();}else if(st.status==='error'){stopPolls();$('loading').textContent='Không quét được Wi-Fi.';showMsg('Hãy thử quét lại. Nếu vẫn lỗi, hãy kiểm tra nguồn Gateway.','error');$('wifi-form').classList.remove('hidden');}}catch(e){}},700);}catch(e){$('loading').textContent='Không thể quét Wi-Fi.';showMsg(e.message+'\nHãy thử lại hoặc tải lại trang.','error');$('wifi-form').classList.remove('hidden');}}
async function showNetworks(){try{const r=await fetch('/scan/results',{cache:'no-store'});if(!r.ok)throw Error();const data=await r.json();const s=$('ssid-list');s.replaceChildren();const unique=new Map();for(const n of data){if(n.ssid&&!unique.has(n.ssid))unique.set(n.ssid,n);}const list=[...unique.values()].sort((a,b)=>b.rssi-a.rssi);if(!list.length){$('loading').textContent='Chưa tìm thấy Wi-Fi.';showMsg('Hãy kiểm tra router, đứng gần Gateway hơn hoặc bấm “Quét Wi-Fi lại”.','error');$('wifi-form').classList.remove('hidden');return;}for(const n of list){const o=document.createElement('option');o.value=n.ssid;const strength=n.rssi>=-60?'Sóng mạnh':(n.rssi>=-75?'Sóng trung bình':'Sóng yếu');o.textContent=n.ssid+' — '+strength;s.appendChild(o);}$('loading').classList.add('hidden');$('wifi-form').classList.remove('hidden');$('msg').className='';$('msg').style.display='none';}catch(e){$('loading').textContent='Lỗi đọc danh sách Wi-Fi.';showMsg('Hãy quét lại.','error');$('wifi-form').classList.remove('hidden');}}
$('rescan-btn').addEventListener('click',loadNetworks);
$('wifi-form').addEventListener('submit',async e=>{e.preventDefault();const b=$('submit-btn');b.disabled=true;b.textContent='Đang kết nối…';$('rescan-btn').disabled=true;showMsg('Đang thử kết nối Wi-Fi. Vui lòng giữ trang này mở.','info');const body=new URLSearchParams({ssid:$('ssid-list').value,pass:$('pass-input').value});try{const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const result=await r.json();if(!r.ok||!result.accepted)throw Error(result.message||'Không thể bắt đầu kết nối');const started=Date.now();connectPoll=setInterval(async()=>{try{const sr=await fetch('/connect-status',{cache:'no-store'});const st=await sr.json();if(st.status==='connected'){stopPolls();$('wifi-form').classList.add('hidden');$('loading').classList.add('hidden');$('success-actions').classList.remove('hidden');$('fallback-hint').classList.add('hidden');showMsg('✅ Wi-Fi đã kết nối thành công. Bạn có thể đóng cửa sổ này và quét mã QR trên tủ điện để mở AquaAuto.','success');return;}if(st.status==='failed'){stopPolls();throw Error(st.message||'Kết nối thất bại. Hãy kiểm tra mật khẩu Wi-Fi.');}if(Date.now()-started>25000){stopPolls();throw Error('Đã hết thời gian chờ. Hãy kiểm tra mật khẩu và thử lại.');}showMsg('Đang kết nối Wi-Fi… '+Math.round((Date.now()-started)/1000)+' giây. Vui lòng chờ.','info');}catch(err){if(err.message){showMsg('❌ '+err.message,'error');b.disabled=false;b.textContent='Thử lại';$('rescan-btn').disabled=false;}}},800);}catch(err){showMsg('❌ '+err.message,'error');b.disabled=false;b.textContent='Thử lại';$('rescan-btn').disabled=false;}});
loadNetworks();
</script></body></html>
)rawliteral";

// ======================== HELPERS ===========================
String makeTopic(const char* suffix) {
  return String("dyauto/") + GATEWAY_ID + "_" + DEVICE_SECRET + "/" + suffix;
}

void setLed(bool on) {
  currentLedState = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void saveWiFiCredentials(const String& ssid, const String& pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
}

void publishStatus() {
  if (!mqttClient.connected()) return;

  StaticJsonDocument<384> doc;
  doc["gateway"] = GATEWAY_ID;
  doc["node"] = 1;
  // Connectivity/app integration test only: state is the Gateway LED simulation.
  // It MUST NOT be interpreted as proof that a real pump/contactor is operating.
  doc["state"] = currentLedState ? "ON" : "OFF";
  doc["state_source"] = "gateway_led_test";
  doc["mode"] = "TEST_ONLY";
  doc["gateway_online"] = true;
  doc["wifi"] = WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED";
  doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  doc["power"] = "UNKNOWN";
  doc["command_id"] = lastCommandId;
  doc["command_result"] = lastCommandResult;
  doc["uptime_s"] = millis() / 1000UL;
  doc["fw"] = "AquaAuto-Gateway-Test-1.0";

  char payload[384];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  if (len && mqttClient.publish(statusTopic.c_str(), payload, true)) {
    lastStatusPublishAt = millis();
  } else {
    Serial.println("MQTT: publish trạng thái thất bại.");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != commandTopic) return;
  if (length == 0 || length > 256) {
    Serial.println("MQTT: bỏ qua payload rỗng hoặc quá dài.");
    lastCommandResult = "rejected_payload_size";
    publishStatus();
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.printf("MQTT JSON không hợp lệ: %s\n", err.c_str());
    lastCommandResult = "rejected_invalid_json";
    publishStatus();
    return;
  }

  if (!doc["node"].is<int>() || doc["node"].as<int>() != 1 || !doc["cmd"].is<const char*>()) {
    Serial.println("MQTT: thiếu node/cmd hoặc sai kiểu dữ liệu.");
    lastCommandResult = "rejected_node_or_cmd";
    publishStatus();
    return;
  }

  const char* cmd = doc["cmd"];
  if (strcmp(cmd, "ON") != 0 && strcmp(cmd, "OFF") != 0) {
    Serial.println("MQTT: lệnh không được hỗ trợ.");
    lastCommandResult = "rejected_unsupported_cmd";
    publishStatus();
    return;
  }

  const unsigned long now = millis();
  if (now - lastCommandAt < COMMAND_COOLDOWN_MS) {
    lastCommandResult = "rejected_cooldown";
    if (doc["command_id"].is<const char*>()) lastCommandId = doc["command_id"].as<String>();
    publishStatus();
    return;
  }

  // Explicitly a test actuator: toggles the onboard LED only.
  setLed(strcmp(cmd, "ON") == 0);
  lastCommandAt = now;
  lastCommandResult = "accepted_test_led";
  if (doc["command_id"].is<const char*>()) lastCommandId = doc["command_id"].as<String>();
  Serial.printf("TEST command: node=1 cmd=%s; onboard LED only, not a real load.\n", cmd);
  publishStatus();
}

void configureMqtt() {
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(384);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(3);
}

void tryMqttReconnect() {
  if (appMode != AppMode::STA_ONLINE || WiFi.status() != WL_CONNECTED || mqttClient.connected()) return;
  unsigned long now = millis();
  if (now - lastMqttAttemptAt < MQTT_RETRY_INTERVAL_MS) return;
  lastMqttAttemptAt = now;
  String clientId = String("DyAuto-") + GATEWAY_ID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  bool ok;
  if (strlen(MQTT_USERNAME)) ok = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  else ok = mqttClient.connect(clientId.c_str());
  if (ok) {
    Serial.println("MQTT: kết nối thành công.");
    if (mqttClient.subscribe(commandTopic.c_str(), 1)) publishStatus();
    else { Serial.println("MQTT subscribe thất bại."); mqttClient.disconnect(); }
  } else {
    Serial.printf("MQTT kết nối thất bại, state=%d\n", mqttClient.state());
  }
}

void configurePortalRoutes() {
  if (portalServerConfigured) return;

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html; charset=utf-8", index_html);
  });

  // Common captive-portal probes used by Android, Apple devices and Windows.
  // Redirecting to the local portal encourages the OS to show its sign-in window;
  // OS behavior varies, so the UI also explains the manual 192.168.4.1 fallback.
  const char* captivePaths[] = {
    "/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
    "/connecttest.txt", "/ncsi.txt", "/success.txt", "/canonical.html"
  };
  for (const char* path : captivePaths) {
    server.on(path, HTTP_GET, []() {
      server.sendHeader("Cache-Control", "no-store");
      server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
      server.send(302, "text/plain; charset=utf-8", "AquaAuto Wi-Fi setup");
    });
  }

  server.on("/scan/start", HTTP_GET, []() {
    if (!portalActive) {
      server.send(503, "application/json", "{\"status\":\"error\",\"message\":\"Portal chưa sẵn sàng\"}");
      return;
    }
    if (scanState == ScanState::RUNNING) {
      server.send(202, "application/json", "{\"status\":\"scanning\"}");
      return;
    }
    if (scanState == ScanState::DONE) {
      server.send(200, "application/json", "{\"status\":\"done\"}");
      return;
    }
    WiFi.scanDelete();
    scanStartedAt = millis();
    int result = WiFi.scanNetworks(true, false); // asynchronous scan; skip hidden SSIDs
    if (result == WIFI_SCAN_FAILED) {
      scanState = ScanState::ERROR;
      server.send(500, "application/json", "{\"status\":\"error\"}");
      return;
    }
    scanState = ScanState::RUNNING;
    server.send(202, "application/json", "{\"status\":\"scanning\"}");
  });

  server.on("/scan/status", HTTP_GET, []() {
    StaticJsonDocument<96> doc;
    const char* state = "idle";
    if (scanState == ScanState::RUNNING) state = "scanning";
    else if (scanState == ScanState::DONE) state = "done";
    else if (scanState == ScanState::ERROR) state = "error";
    doc["status"] = state;
    String json; serializeJson(doc, json);
    server.send(200, "application/json; charset=utf-8", json);
  });

  server.on("/scan/results", HTTP_GET, []() {
    if (scanState != ScanState::DONE) {
      server.send(409, "application/json", "{\"error\":\"scan_not_ready\"}");
      return;
    }
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    int count = WiFi.scanComplete();
    if (count > 0) {
      for (int i = 0; i < count; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        JsonObject item = arr.createNestedObject();
        item["ssid"] = ssid;
        item["rssi"] = WiFi.RSSI(i);
      }
    }
    String json; serializeJson(doc, json);
    WiFi.scanDelete();
    scanState = ScanState::IDLE;
    server.send(200, "application/json; charset=utf-8", json);
  });

  server.on("/save", HTTP_POST, []() {
    if (!portalActive || appMode != AppMode::AP_CONFIG) {
      server.send(409, "application/json", "{\"accepted\":false,\"message\":\"Thiết bị đang xử lý kết nối. Hãy chờ hoặc tải lại trang.\"}");
      return;
    }
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 63) {
      server.send(400, "application/json", "{\"accepted\":false,\"message\":\"SSID hoặc mật khẩu không hợp lệ.\"}");
      return;
    }
    WiFi.scanDelete();
    scanState = ScanState::IDLE;
    pendingSSID = ssid;
    pendingPassword = pass;
    portalConnectAttempt = true;
    saveCredentialsOnConnect = true;
    portalResult = PortalResult::CONNECTING;
    connectionMessage = "Đang kết nối Wi-Fi...";
    wifiAttemptStartedAt = millis();
    portalCloseAt = 0;
    appMode = AppMode::STA_CONNECTING;
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(false, false);
    WiFi.begin(pendingSSID.c_str(), pendingPassword.c_str());
    server.send(202, "application/json", "{\"accepted\":true}");
  });

  server.on("/connect-status", HTTP_GET, []() {
    StaticJsonDocument<192> doc;
    if (portalResult == PortalResult::SUCCESS) doc["status"] = "connected";
    else if (portalResult == PortalResult::FAILED) {
      doc["status"] = "failed";
      doc["message"] = connectionMessage;
    } else if (portalResult == PortalResult::CONNECTING) doc["status"] = "connecting";
    else doc["status"] = "idle";
    String json; serializeJson(doc, json);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json; charset=utf-8", json);
  });

  server.onNotFound([]() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
    server.send(302, "text/plain; charset=utf-8", "AquaAuto Wi-Fi setup");
  });

  portalServerConfigured = true;
}

void startAccessPoint() {
  if (portalActive) return;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  bool secured = strlen(AP_PASSWORD) >= 12 && strcmp(AP_PASSWORD, "12345678") != 0 && strcmp(AP_PASSWORD, "CHANGE_ME_123") != 0;
  bool apOk = secured ? WiFi.softAP(AP_SSID, AP_PASSWORD) : WiFi.softAP(AP_SSID);
  if (!secured) Serial.println("CẢNH BÁO BẢO MẬT: AP cấu hình đang mở vì mật khẩu mặc định/yếu. Chỉ dùng thử nghiệm; hãy đặt mật khẩu riêng tối thiểu 12 ký tự trước khi triển khai.");
  if (!apOk) {
    Serial.println("Không thể khởi tạo Access Point.");
    return;
  }

  configurePortalRoutes();
  dnsServer.start(DNS_PORT, "*", AP_IP);
  server.begin();
  portalActive = true;
  appMode = AppMode::AP_CONFIG;
  portalResult = PortalResult::IDLE;
  portalCloseAt = 0;
  scanState = ScanState::IDLE;
  Serial.printf("Captive Portal: SSID=%s, IP=%s\n", AP_SSID, AP_IP.toString().c_str());
}

void closeAccessPoint() {
  if (!portalActive) return;
  Serial.println("Đóng Wi-Fi cấu hình sau thời gian chờ an toàn.");
  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  portalActive = false;
  scanState = ScanState::IDLE;
  if (WiFi.status() == WL_CONNECTED) WiFi.mode(WIFI_STA);
}

void beginWiFiAttempt(const String& ssid, const String& pass, bool fromPortal) {
  pendingSSID = ssid;
  pendingPassword = pass;
  portalConnectAttempt = fromPortal;
  saveCredentialsOnConnect = fromPortal;
  connectionMessage = "Đang kết nối Wi-Fi...";
  if (fromPortal) portalResult = PortalResult::CONNECTING;
  wifiAttemptStartedAt = millis();
  appMode = AppMode::STA_CONNECTING;
  WiFi.mode(fromPortal ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(pendingSSID.c_str(), pendingPassword.c_str());
}

void processWiFiState() {
  if (appMode != AppMode::STA_CONNECTING) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (saveCredentialsOnConnect) saveWiFiCredentials(pendingSSID, pendingPassword);
    saveCredentialsOnConnect = false;
    connectionMessage = "Kết nối Wi-Fi thành công.";
    Serial.printf("Wi-Fi connected, IP=%s, RSSI=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    portalConnectAttempt = false;
    appMode = AppMode::STA_ONLINE;
    if (portalActive && portalResult == PortalResult::CONNECTING) {
      portalResult = PortalResult::SUCCESS;
      portalCloseAt = millis() + PORTAL_SUCCESS_GRACE_MS;
    }
    configureMqtt();
    lastMqttAttemptAt = millis() - MQTT_RETRY_INTERVAL_MS;
    return;
  }

  if (millis() - wifiAttemptStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
    WiFi.disconnect(false, false);
    connectionMessage = "Không kết nối được Wi-Fi. Kiểm tra mật khẩu và thử lại.";
    Serial.println(connectionMessage);
    pendingPassword = ""; // Do not keep a failed password in RAM longer than necessary.
    saveCredentialsOnConnect = false;
    portalConnectAttempt = false;
    if (portalActive) {
      appMode = AppMode::AP_CONFIG;
      portalResult = PortalResult::FAILED;
    } else {
      // Saved network failed: preserve saved credentials and expose the setup portal.
      startAccessPoint();
      appMode = portalActive ? AppMode::AP_CONFIG : AppMode::STA_CONNECTING;
      portalResult = PortalResult::IDLE;
      lastWiFiRetryAt = millis();
    }
  }
}

void processPortal() {
  if (portalActive) {
    dnsServer.processNextRequest();
    server.handleClient();

    if (scanState == ScanState::RUNNING) {
      int result = WiFi.scanComplete();
      if (result >= 0) scanState = ScanState::DONE;
      else if (result == WIFI_SCAN_FAILED || millis() - scanStartedAt >= WIFI_SCAN_TIMEOUT_MS) {
        WiFi.scanDelete();
        scanState = ScanState::ERROR;
      }
    }

    if (portalResult == PortalResult::SUCCESS && portalCloseAt != 0 &&
        (long)(millis() - portalCloseAt) >= 0) {
      closeAccessPoint();
    }
  }
}

void eraseWiFiCredentialsAndRestart() {
  Serial.println("Xóa cấu hình Wi-Fi và khởi động lại...");
  preferences.begin("wifi", false);
  preferences.remove("ssid");
  preferences.remove("pass");
  preferences.end();
  delay(100);
  ESP.restart();
}

void processButton() {
  unsigned long now = millis();
  int reading = digitalRead(BUTTON_PIN);
  if (reading != rawButtonLast) {
    rawButtonLast = reading;
    lastButtonChangeAt = now;
  }
  if (now - lastButtonChangeAt < BUTTON_DEBOUNCE_MS) return;

  if (reading != stableButtonState) {
    stableButtonState = reading;
    if (stableButtonState == LOW) {
      buttonDownAt = now;
      longPressHandled = false;
    } else if (!longPressHandled && now - buttonDownAt < BUTTON_HOLD_MS) {
      setLed(!currentLedState);
      publishStatus();
    }
  }
  if (stableButtonState == LOW && !longPressHandled && now - buttonDownAt >= BUTTON_HOLD_MS) {
    longPressHandled = true;
    eraseWiFiCredentialsAndRestart();
  }
}

// ======================== SETUP / LOOP ======================
void setup() {
  Serial.begin(115200);
  Serial.println("AquaAuto Gateway connectivity test firmware. MQTT commands control onboard LED only; NOT a real pump/relay.");
  WiFi.persistent(false); // Preferences is the source of truth for saved credentials.
  WiFi.setAutoReconnect(true);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  statusTopic = makeTopic("status");
  commandTopic = makeTopic("command");
  Serial.printf("MQTT status topic: %s\n", statusTopic.c_str());
  Serial.printf("MQTT command topic: %s\n", commandTopic.c_str());

  preferences.begin("wifi", true);
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  preferences.end();

  if (!savedSSID.isEmpty()) {
    Serial.printf("Đang kết nối Wi-Fi đã lưu: %s\n", savedSSID.c_str());
    beginWiFiAttempt(savedSSID, savedPass, false);
  } else {
    startAccessPoint();
  }
}

void loop() {
  processPortal();
  processWiFiState();

  if (appMode == AppMode::STA_ONLINE) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi mất kết nối; thử kết nối lại.");
      mqttClient.disconnect();
      WiFi.reconnect();
      appMode = AppMode::STA_CONNECTING;
      portalConnectAttempt = false;
      saveCredentialsOnConnect = false;
      wifiAttemptStartedAt = millis();
      lastWiFiRetryAt = millis();
    } else {
      tryMqttReconnect();
      if (mqttClient.connected()) {
        mqttClient.loop();
        if (millis() - lastStatusPublishAt >= STATUS_INTERVAL_MS) publishStatus();
      }
    }
  }

  // If a reconnect to the saved network fails, processWiFiState falls back to AP mode.
  processButton();
}
