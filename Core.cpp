#include "Core.h"
#include "ESP8266NetBIOS.h"

using namespace std;
using namespace std::placeholders;		

/* Soft AP network parameters */
IPAddress apIP(192,168,4,1);
IPAddress netMsk(255, 255, 255, 0);
IPAddress lanIp;			// Надо сделать настройки ip адреса
IPAddress gateway;

CoreClass::CoreClass() : TaskController(20000), AsyncWebHandler(){
	initWiFi();
	//onRun(bind(&CoreClass::connectSTA, CORE));					/* Пытаемся соедениться с точкой доступа каждые 20 секунд */
}

void CoreClass::begin(){
	init();
	initServer();	
}

void CoreClass::init(){
	POWER = new PowerClass();
	Serial.begin(115200);
	Serial.println("OK");
	SPIFFS.begin();
	
	_downloadHTTPAuth();	
	_downloadSettings();
	
	POWER->setInterval(_settings.time_off);
	POWER->onRun(bind(&CoreClass::exit,CORE));
	
	BATTERY = new BatteryClass(&_settings.bat_max, 20000);
	if(BATTERY->callibrated()){		
		saveSettings();
	};
	
	BLINK = new BlinkClass();
	
	SCALES = new ScalesClass(&server);
	CONNECT = new Task(bind(&CoreClass::connectSTA, CORE),20000);
	add(CONNECT);
	add(POWER);
	add(BATTERY);
	add(BLINK);
	//add(SCALES);
}

void CoreClass::initWiFi(){
	StModeConnectedHandler = WiFi.onStationModeConnected(bind(&CoreClass::onSTAModeConnected, CORE, _1));
	StModeDisconnectedHandler = WiFi.onStationModeDisconnected(bind(&CoreClass::onSTAModeDisconnected, CORE, _1));
		
	WiFi.persistent(false);
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	//WiFi.smartConfigDone();
	WiFi.hostname(MY_HOST_NAME);
	WiFi.mode(WIFI_AP_STA);
	WiFi.softAPConfig(apIP, apIP, netMsk);
	WiFi.softAP(SOFT_AP_SSID, SOFT_AP_PASSWORD);	
}

void CoreClass::initServer(){
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(DNS_PORT, "*", apIP);
	setAuthentication(_httpAuth.wwwUsername.c_str(), _httpAuth.wwwPassword.c_str());
	
	server.addHandler(CORE);	
	server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
	server.addHandler(new SPIFFSEditor(_httpAuth.wwwUsername.c_str(), _httpAuth.wwwPassword.c_str()));
	server.addHandler(new HttpUpdaterClass("sa", F("654321")));
	
	server.on("/rc", bind(&CoreClass::reconnectWifi, CORE, _1));																		/* Пересоединиться по WiFi. */
	server.on("/sv", bind(&CoreClass::handleScaleProp, CORE, _1));																												/* Получить значения. */
	server.on("/settings.json",HTTP_ANY, [this](AsyncWebServerRequest * reguest){
		if (!isAuthentified(reguest))
			return reguest->requestAuthentication();		
		reguest->send(SPIFFS, reguest->url());	
	});
	server.on("/secret.json",[this](AsyncWebServerRequest * reguest){
		if (!reguest->authenticate(_httpAuth.wwwUsername.c_str(), _httpAuth.wwwPassword.c_str()))
			return reguest->requestAuthentication();		
		reguest->send(SPIFFS, reguest->url());
	});
	#ifdef HTML_PROGMEM
		server.on("/",[](AsyncWebServerRequest * reguest){	reguest->send_P(200,F("text/html"),index_html);});								/* Главная страница. */
		server.rewrite("/sn", "/settings.html");
		server.serveStatic("/", SPIFFS, "/");
	#else	
		server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
	#endif	
	server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});
	server.begin();
	connectSTA();
}

void CoreClass::connectSTA(){
	WiFi.disconnect(false);
	/*!  */
	int n = WiFi.scanComplete();
	if(n == -2){
		n = WiFi.scanNetworks();
		if (n <= 0){
			goto scan;
		}
	}else if (n > 0){
		goto connect;
	}else{
		goto scan;
	}
	connect:
	if (_settings.wSSID.length()==0){
		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(false);
		CONNECT->pause();
		return;
	}
	for (int i = 0; i < n; ++i)	{
		if(WiFi.SSID(i) == _settings.wSSID.c_str()){
			String ssid_scan;
			int32_t rssi_scan;
			uint8_t sec_scan;
			uint8_t* BSSID_scan;
			int32_t chan_scan;
			bool hidden_scan;
			WiFi.setAutoConnect(true);
			WiFi.setAutoReconnect(true);
			WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
			if (!_settings.autoIp){
				if (lanIp.fromString(_settings.scaleLanIp) && gateway.fromString(_settings.scaleGateway)){					
					WiFi.config(lanIp,gateway, netMsk);									// Надо сделать настройки ip адреса
				}
			}
			WiFi.softAP(SOFT_AP_SSID, SOFT_AP_PASSWORD, chan_scan); //Устанавливаем канал как роутера
			WiFi.begin ( _settings.wSSID.c_str(), _settings.wKey.c_str(),chan_scan,BSSID_scan);
			int status = WiFi.waitForConnectResult();
			if(status == WL_CONNECTED ){
				NBNS.begin(MY_HOST_NAME);
			}
			return;
		}
	}
	scan:
	WiFi.scanDelete();
	WiFi.scanNetworks(true);
}

void CoreClass::reconnectWifi(AsyncWebServerRequest * request){
	AsyncWebServerResponse *response = request->beginResponse_P(200, PSTR("text/html"), "RECONNECT...");
	response->addHeader("Connection", "close");
	request->onDisconnect([](){
		SPIFFS.end();
		ESP.reset();
	});
	request->send(response);
}

void CoreClass::exit(){
	//browserServer.stop();
	SPIFFS.end();
	//Scale.power_down(); /// Выключаем ацп	
	POWER->off();
	ESP.reset();
}

bool CoreClass::saveSettings() {
	
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	
	JsonObject& scale = json.createNestedObject(SCALE_JSON);
	scale["id_n_admin"] = _settings.scaleName;
	scale["id_p_admin"] = _settings.scalePass;
	scale["id_auto"] = _settings.autoIp;
	scale["id_lan_ip"] = _settings.scaleLanIp;
	scale["id_gateway"] = _settings.scaleGateway;
	scale["id_subnet"] = _settings.scaleSubnet;
	scale["id_ssid"] = _settings.wSSID;
	scale["id_key"] = _settings.wKey;
	scale["bat_max"] = _settings.bat_max;
	scale["id_pe"] = _settings.power_time_enable;
	scale["id_pt"] = _settings.time_off;
	
	JsonObject& server = json.createNestedObject(SERVER_JSON);
	server["id_host"] = _settings.hostUrl;
	server["id_pin"] = _settings.hostPin;
	server["timeout"] = _settings.timeout;
	
	File serverFile = SPIFFS.open(SETTINGS_FILE, "w");
	if (!serverFile) {
		//serverFile.close();
		return false;
	}

	json.printTo(serverFile);
	serverFile.flush();
	serverFile.close();
	return true;
}

bool CoreClass::_downloadSettings() {
	_settings.scaleName = "admin";
	_settings.scalePass = "1234";
	_settings.autoIp = true;
	_settings.scaleLanIp = "192.168.1.100";
	_settings.scaleGateway = "192.168.1.1";
	_settings.scaleSubnet = "255.255.255.0";
	_settings.hostUrl = HOST_URL;
	_settings.hostPin = 0;
	_settings.timeout = TIMEOUT_HTTP;
	_settings.bat_max = MIN_CHG;
	_settings.power_time_enable = false;
	_settings.time_off = 2400000;
	File serverFile = SPIFFS.open(SETTINGS_FILE, "r");
	if (!serverFile) {
		return false;
	}
	size_t size = serverFile.size();

	// Allocate a buffer to store contents of the file.
	unique_ptr<char[]> buf(new char[size]);
	
	serverFile.readBytes(buf.get(), size);
	serverFile.close();
	DynamicJsonBuffer jsonBuffer(size);
	JsonObject& json = jsonBuffer.parseObject(buf.get());

	if (!json.success()) {
		return false;
	}
	if (json.containsKey(SCALE_JSON)){
		_settings.scaleName = json[SCALE_JSON]["id_n_admin"].as<String>();
		_settings.scalePass = json[SCALE_JSON]["id_p_admin"].as<String>();
		_settings.autoIp = json[SCALE_JSON]["id_auto"];
		_settings.scaleLanIp = json[SCALE_JSON]["id_lan_ip"].as<String>();
		_settings.scaleGateway = json[SCALE_JSON]["id_gateway"].as<String>();
		_settings.scaleSubnet = json[SCALE_JSON]["id_subnet"].as<String>();
		_settings.wSSID = json[SCALE_JSON]["id_ssid"].as<String>();
		_settings.wKey = json[SCALE_JSON]["id_key"].as<String>();
		_settings.bat_max = json[SCALE_JSON]["bat_max"];
		_settings.power_time_enable = json[SCALE_JSON]["id_pe"];
		_settings.time_off = json[SCALE_JSON]["id_pt"];
	}
	if (json.containsKey(SERVER_JSON)){
		_settings.hostUrl = json[SERVER_JSON]["id_host"].as<String>();
		_settings.hostPin = json[SERVER_JSON]["id_pin"];
		_settings.timeout = json[SERVER_JSON]["timeout"];
	}
	return true;
}

void CoreClass::handle(){
	run();
	dnsServer.processNextRequest();
	//POWER->checkButtonPressed();
	if (SCALES->isSave()){
		saveEvent("weight", String(SCALES->getSaveValue())+"_kg");
		SCALES->setIsSave(false);
	}
}

void CoreClass::onSTAModeConnected(const WiFiEventStationModeConnected& evt) {
	CONNECT->pause();
	BLINK->_flash = 50;
	BLINK->_blink = 3000;
}

void CoreClass::onSTAModeDisconnected(const WiFiEventStationModeDisconnected& evt) {
	WiFi.scanDelete();
	WiFi.scanNetworks(true);
	CONNECT->resume();
	BLINK->_flash = 500;
	BLINK->_blink = 500;
}

bool CoreClass::canHandle(AsyncWebServerRequest *request){
	if(request->url().equalsIgnoreCase("/settings.html")){
		goto auth;
	}
#ifndef HTML_PROGMEM
	else if(request->url().equalsIgnoreCase("/sn")){
		goto auth;
	}
#endif
	else
		return false;
	auth:
	if (!request->authenticate(_settings.scaleName.c_str(), _settings.scalePass.c_str())){
		if(!request->authenticate(_username.c_str(), _password.c_str())){
			request->requestAuthentication();
			return false;
		}
	}
	return true;
}

void CoreClass::handleRequest(AsyncWebServerRequest *request){
	if (request->args() > 0){
		String message = " ";
		if (request->hasArg("ssid")){
			if (request->hasArg("auto"))
				_settings.autoIp = true;
			else
				_settings.autoIp = false;
			_settings.scaleLanIp = request->arg("lan_ip");
			_settings.scaleGateway = request->arg("gateway");
			_settings.scaleSubnet = request->arg("subnet");
			_settings.wSSID = request->arg("ssid");
			if(_settings.wSSID.length()>0){
				CONNECT->resume();
			}else{
				CONNECT->pause();
			}
			_settings.wKey = request->arg("key");
			goto save;
		}
		if(request->hasArg("data")){
			RTC.setDateTime(request->arg("data"));
			request->send(200, F("text/html"), RTC.getDateTime());
			return;
		}
		if (request->hasArg("host")){
			_settings.hostUrl = request->arg("host");
			_settings.hostPin = request->arg("pin").toInt();
			goto save;
		}
		if (request->hasArg("n_admin")){
			_settings.scaleName = request->arg("n_admin");
			_settings.scalePass = request->arg("p_admin");
			goto save;
		}
		if (request->hasArg("pt")){
			if (request->hasArg("pe"))
				POWER->enabled = _settings.power_time_enable = true;
			else
				POWER->enabled = _settings.power_time_enable = false;
			_settings.time_off = request->arg("pt").toInt();
			goto save;
		}
		save:
		if (saveSettings()){
			goto url;
		}
		return request->send(400);
	}
	url:
	#ifdef HTML_PROGMEM
		request->send_P(200,F("text/html"), settings_html);
	#else
		if(request->url().equalsIgnoreCase("/sn"))
			request->send_P(200, F("text/html"), netIndex);
		else
			request->send(SPIFFS, request->url());
	#endif
}

bool CoreClass::_downloadHTTPAuth() {
	_httpAuth.wwwUsername = "sa";
	_httpAuth.wwwPassword = "343434";
	File configFile = SPIFFS.open(SECRET_FILE, "r");
	if (!configFile) {
		configFile.close();
		return false;
	}
	size_t size = configFile.size();

	unique_ptr<char[]> buf(new char[size]);
	
	configFile.readBytes(buf.get(), size);
	configFile.close();
	DynamicJsonBuffer jsonBuffer(256);
	JsonObject& json = jsonBuffer.parseObject(buf.get());

	if (!json.success()) {
		return false;
	}
	_httpAuth.wwwUsername = json["user"].as<String>();
	_httpAuth.wwwPassword = json["pass"].as<String>();
	return true;
}

bool CoreClass::_saveHTTPAuth() {
	
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	json["user"] = _httpAuth.wwwUsername;
	json["pass"] = _httpAuth.wwwPassword;

	//TODO add AP data to html
	File configFile = SPIFFS.open(SECRET_FILE, "w");
	if (!configFile) {
		configFile.close();
		return false;
	}

	json.printTo(configFile);
	configFile.flush();
	configFile.close();
	return true;
}

bool CoreClass::checkAdminAuth(AsyncWebServerRequest * r) {
	return r->authenticate(_httpAuth.wwwUsername.c_str(), _httpAuth.wwwPassword.c_str());
}

bool CoreClass::isAuthentified(AsyncWebServerRequest * request){
	if (!request->authenticate(_settings.scaleName.c_str(), _settings.scalePass.c_str())){
		if (!checkAdminAuth(request)){
			return false;
		}
	}
	return true;
}

void CoreClass::handleScaleProp(AsyncWebServerRequest * request){
	if (!isAuthentified(request))
		return request->requestAuthentication();
	AsyncJsonResponse * response = new AsyncJsonResponse();
	JsonObject& root = response->getRoot();
	root["id_date"] = RTC.getDateTime();
	root["id_local_host"] = String(MY_HOST_NAME);
	root["id_ap_ssid"] = String(SOFT_AP_SSID);
	root["id_ap_ip"] = toStringIp(WiFi.softAPIP());
	root["id_ip"] = toStringIp(WiFi.localIP());
	root["sl_id"] = String(SCALES->getSeal());					
	root["chip_v"] = String(ESP.getCpuFreqMHz());
	root["id_mac"] = WiFi.macAddress();
	response->setLength();
	request->send(response);
}

String CoreClass::getHash(const int code, const String& date, const String& type, const String& value){
	
	String event = String(code);
	event += "\t" + date + "\t" + type + "\t" + value;
	int s = 0;
	for (int i = 0; i < event.length(); i++)
		s += event[i];
	event += (char) (s % 256);
	String hash = "";
	for (int i = 0; i < event.length(); i++){
		int c = (event[i] - (i == 0? 0 : event[i - 1]) + 256) % 256;
		int c1 = c / 16; int c2 = c % 16;
		char d1 = c1 < 10? '0' + c1 : 'a' + c1 - 10;
		char d2 = c2 < 10? '0' + c2 : 'a' + c2 - 10;
		hash += "%"; hash += d1; hash += d2;
	}
	return hash;
}

bool CoreClass::eventToServer(const String& date, const String& type, const String& value){
	if(_settings.hostPin == 0)
	return false;
	HTTPClient http;
	String message = "http://";
	message += _settings.hostUrl.c_str();
	String hash = getHash(_settings.hostPin, date, type, value);
	message += "/scales.php?hash=" + hash;
	http.begin(message);
	http.setTimeout(_settings.timeout);
	int httpCode = http.GET();
	http.end();
	if(httpCode == HTTP_CODE_OK) {
		return true;
	}
	return false;
}

bool CoreClass::saveEvent(const String& event, const String& value) {
	String date = RTC.getDateTime();
	bool flag = WiFi.status() == WL_CONNECTED?eventToServer(date, event, value):false;
	File readFile;
	readFile = SPIFFS.open("/events.json", "r");
    if (!readFile) {  
		return false;
    }
	
    size_t size = readFile.size(); 	
    unique_ptr<char[]> buf(new char[size]);
    readFile.readBytes(buf.get(), size);	
    readFile.close();
		
    DynamicJsonBuffer jsonBuffer(JSON_ARRAY_SIZE(110));
	//DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(buf.get());

    if (!json.containsKey(EVENTS_JSON)) {	
		json["cur_num"] = 0;
		json["max_events"] = MAX_EVENTS;
		JsonArray& events = json.createNestedArray(EVENTS_JSON);
		for(int i = 0; i < MAX_EVENTS; i++){
			JsonObject& ev = jsonBuffer.createObject();
			ev["d"] = "";
			ev["v"] = "";
			ev["s"] = false;
			events.add(ev);	
		}		
		/*if (!json.success())
			return false;*/
    }
	
	long n = json["cur_num"];
	
	json[EVENTS_JSON][n]["d"] = date;
	json[EVENTS_JSON][n]["v"] = value;	
	json[EVENTS_JSON][n]["s"] = flag;
		
	if ( ++n == MAX_EVENTS){
		n = 0;
	}
	json["max_events"] = MAX_EVENTS;
	json["cur_num"] = n;
	//TODO add AP data to html
	File saveFile = SPIFFS.open("/events.json", "w");
	if (!saveFile) {
		//SPIFFS.remove("/events.json");
		//saveFile.close();
		return false;
	}

	json.printTo(saveFile);
	saveFile.flush();
	saveFile.close();
	return true;
}

int BatteryClass::_get_adc(byte times){
	long sum = 0;
		for (byte i = 0; i < times; i++) {
			sum += analogRead(A0);
		}
		return times == 0?sum :sum / times;
}

bool BatteryClass::callibrated(){
			bool flag = false;
			int charge = _get_adc();
			int t = constrain(*_max, MIN_CHG, 1024);
			//_max = constrain(t, MIN_CHG, 1024);
			if(t != *_max)
				flag = true;
			*_max = t;
			if(t == *_max){
				*_max +=1;
			}
			charge = constrain(charge, MIN_CHG, 1024);
			if (*_max < charge){
				*_max = charge;
				flag = true;
			}
			return flag;
		};

/** IP to String? */
String toStringIp(IPAddress ip) {
	String res = "";
	for (int i = 0; i < 3; i++) {
		res += String((ip >> (8 * i)) & 0xFF) + ".";
	}
	res += String(((ip >> 8 * 3)) & 0xFF);
	return res;
}

AsyncWebServer server(80);
DNSServer dnsServer;
CoreClass * CORE;
Task * CONNECT;
PowerClass * POWER;
BatteryClass * BATTERY;
BlinkClass * BLINK;
DataTimeClass RTC;

