// CoreScales.h

#ifndef _CORESCALES_h
#define _CORESCALES_h

//#include "Arduino.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <IPAddress.h>
//#include <ArduinoJson.h>
//#include <AsyncJson.h>
#include <DNSServer.h>
#include <FS.h>
#include <functional>
#include <RtcDS1307.h>
#include <Wire.h>
#include <ESP8266HTTPClient.h>
#include "TaskController.h"
#include "Task.h"
#include "HttpUpdater.h"
#include "Scales.h"
#include "CoreMemory.h"

#define HTML_PROGMEM						//Использовать веб страницы из flash памяти

#define countof(a) (sizeof(a) / sizeof(a[0]))

#define EN_NCP				12				/* сигнал включения питания  */
#define PWR_SW				13				/* сигнал от кнопки питания */
#define LED					2				/* индикатор работы */
#define PIN_ON_5V			15				/* Сигнал включения питания АЦП 5 вольт */

#define MY_HOST_NAME		"scl"
#define SOFT_AP_SSID		"SCALES"
#define SOFT_AP_PASSWORD	"12345678"

#define SETTINGS_FILE		"/settings.json"
#define SECRET_FILE			"/secret.json"
#define HOST_URL			"sdb.net.ua"
#define TIMEOUT_HTTP		3000
#define MAX_EVENTS			100
//#define MIN_CHG				880				//ADC = (Vin * 1024)/Vref  Vref = 1V
#define MIN_CHG				670				//ADC = (Vin * 1024)/Vref  Vref = 1V

#define SCALE_JSON			"scale"
#define SERVER_JSON			"server"
#define EVENTS_JSON			"events"

#define DNS_PORT			53
//IPAddress lanIp;			// Надо сделать настройки ip адреса
//IPAddress gateway;

//index.html
const char index_html[] PROGMEM = R"(<!DOCTYPE html><html lang='en'><head> <meta charset='UTF-8'> <meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1.0, user-scalable=no'/> <meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'/> <meta http-equiv='Pragma' content='no-cache'/> <title>WEB SCALES</title> <link rel='stylesheet' type='text/css' href='global.css'> <link rel='shortcut icon' href='favicon.png' type='image/png'> <style>#w_style{background: #fff; font-size: 80px; font-family: Arial, sans-serif; color: #618ad2; margin-left: auto; margin-right: auto;}table{width: 100%;}input{font-size: 20px; text-align: center;}</style> <script>var w; var d=document; function ScalesSocket(h, p, fm, fe){var host=h; var protocol=p; var timerWeight; var timerSocket; var ws; var startWeightTimeout=function(){clearTimeout(timerWeight); timerWeight=setTimeout(function(){fe();}, 5000);}; this.getWeight=function(){ws.send('/wt'); startWeightTimeout();}; this.openSocket=function(){ws=new WebSocket(host, protocol); ws.onopen=this.getWeight; ws.onclose=function(e){clearTimeout(timerWeight); starSocketTimeout(); fe();}; ws.onerror=function(e){fe();}; ws.onmessage=function(e){fm(JSON.parse(e.data));}}; var starSocketTimeout=function(){clearTimeout(timerSocket); timerSocket=setTimeout(function(){this.prototype.openSocket();}, 5000);}}function go(){d.getElementById('weight').innerHTML='---'; d.getElementById('charge_id').innerHTML='--%'; d.getElementById('buttonZero').style.visibility='visible';}function setZero(){d.getElementById('buttonZero').style.visibility='hidden'; var r=new XMLHttpRequest(); d.getElementById('weight').innerHTML='...'; r.onreadystatechange=function(){if (r.readyState===4 && r.status===204){d.getElementById('buttonZero').style.visibility='visible'; w.getWeight();}}; r.open('GET', '/tp', true); r.timeout=5000; r.ontimeout=function(){go();}; r.onerror=function(){go();}; r.send(null);}window.onload=function(){onBodyLoad();}; function onBodyLoad(){w=new ScalesSocket('ws://' + d.location.host + '/ws', ['arduino'], function(e){try{d.getElementById('weight').innerHTML=e.w; d.getElementById('charge_id').innerHTML=e.c + '%'; if (e.s){d.getElementById('w_style').setAttribute('style', 'background: #fff;');}else{d.getElementById('w_style').setAttribute('style', 'background: #C4C4C4;');}}catch (e){go();}finally{w.getWeight();}}, function(){go(); w.openSocket();}); w.openSocket();}</script></head><body><div align='center'> <table> <tr> <td><img src='scales.png' style='height: 42px;'/></td><td align='right'> <h3 id='brand_name'>scale.in.ua</h3></td></tr></table> <p hidden='hidden' id='dnt'></p></div><hr><div align='right' id='w_style'><b id='weight'>---</b></div><hr><table> <tr> <td style='width:1%; white-space: nowrap'><img src='battery.png' alt='B'/></td><td> <h3 id='charge_id' style='display: inline'>--%</h3></td><td align='right'><b><a href='javascript:setZero()' id='buttonZero' class='btn btn--s btn--blue'>&gt;0&lt;</a></b></td></tr></table><hr><table> <tr> <td><a href='/events.html' class='btn btn--m btn--blue'>события</a> <br></td></tr><tr> <td> <br/><a href='/settings.html'>настройки</a> <br></td></tr></table><hr><footer align='center'>2018 © Powered by www.scale.in.ua</footer></body></html>)";



const char netIndex[] PROGMEM = R"(<html lang='en'><meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1'/><body><form method='POST'><input name='ssid'><br/><input type='password' name='key'><br/><input type='submit' value='СОХРАНИТЬ'></form></body></html>)";

//settings.html
const char settings_html[] PROGMEM = R"(<!DOCTYPE html><html lang='en'><head> <meta charset='UTF-8'> <meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1.0, user-scalable=no'/> <meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'/> <meta http-equiv='Pragma' content='no-cache'/> <title>Настройки</title> <link rel='stylesheet' type='text/css' href='global.css'> <style>input:focus{background: #FA6; outline: none;}table{width: 100%;}input, select{width: 100%; text-align: right; font-size: 18px;}input[type=submit]{width: auto;}input[type=checkbox]{width: auto;}</style> <script>window.onload=function (){load('settings.js', 'js', function (){GetSettings();});}; function load(e, t, n){var d=document; if ('js'===t){var a=d.createElement("script"); a.src=e, a.type='text/javascript', a.async=!1, a.onload=function (){n()}, d.getElementsByTagName('head')[0].appendChild(a)}else if ('css'===t){var a=d.createElement('link'); a.href=e, a.rel='stylesheet', a.type='text/css', a.async=!1, a.onload=function (){n()}, d.getElementsByTagName('head')[0].appendChild(a)}}</script></head><body style='visibility: hidden'><a href='/' class='btn btn--s btn--blue'>&lt;</a>&nbsp;&nbsp;<strong>Настройки</strong><hr><fieldset> <details> <summary>Конфигурация сети</summary> <br><h5 align='left'><b>Точка доступа WiFi роутера</b></h5> <form id='form_id' action='javascript:formNet("form_id")'> Получать IP: <input type='checkbox' id='id_auto' name='auto' onclick='enableAuthFields(this);'> <div id='id_ip'></div><table id='id_table_net'> <tr> <td>IP:</td><td> <input id='id_lan_ip' type='text' name='lan_ip' onfocus='setOnBlur(this)'> </td></tr><tr> <td>ШЛЮЗ:</td><td> <input id='id_gateway' type='text' name='gateway' onfocus='setOnBlur(this)'> </td></tr><tr> <td>МАСКА:</td><td> <input id='id_subnet' type='text' name='subnet' onfocus='setOnBlur(this)'> </td></tr></table> <table> <tr> <td>СЕТЬ:</td><td> <input id='id_ssid' name='ssid' placeholder='имя сети'> </td></tr><tr> <td>КЛЮЧ:</td><td> <input id='id_key' type='password' name='key' placeholder='пароль'> </td></tr><tr> <td></td><td> <input type='submit' value='СОХРАНИТЬ'/> </td></tr></table> </form> </details></fieldset><br/><fieldset style='width: auto'> <details> <summary>Общии настройки</summary> <br><form action='javascript:sendDateTime()'> <h5 align='left'><b>Установка дата время</b></h5> <table> <tr> <td> <h5 id='id_date'>дата время</h5></td><td> <input type='submit' name='data' value='УСТАНОВИТЬ'/> </td></tr></table> </form> <hr> <form method='POST'> <h5 align='left'><b>Время выключения</b></h5> <table> <tr> <td> <input type='checkbox' id='id_pe' name='pe'> </td><td> <select id='id_pt' name='pt' title="Время выключения"> <option value='600000'>10мин</option> <option value='1200000'>20мин</option> <option value='1800000'>30мин</option> <option value='2400000'>40мин</option> </select> </td><td> <input type='submit' value='УСТАНОВИТЬ'/> </td></tr></table> </form> <hr> <form method='POST'> <h5>Настройки база данных интернет</h5> <table> <tr> <td>СЕРВЕР:</td><td> <input id='id_host' name='host' placeholder='сервер'> </td></tr><tr> <td>ПИН:</td><td> <input id='id_pin' name='pin' placeholder='пин весов'> </td></tr><tr> <td><a href='javascript:openSDB();'>открыть</a></td><td> <input id='id_submit_code' type='submit' value='СОХРАНИТЬ'/> </td></tr></table> </form> <hr> <form method='POST'> <h5>Доступ к настройкам</h5> <table> <tr> <td>ИМЯ:</td><td> <input id='id_n_admin' name='n_admin' placeholder='имя админ'> </td></tr><tr> <td>ПАРОЛЬ:</td><td> <input type='password' id='id_p_admin' name='p_admin' placeholder='пароль админ'> </td></tr><tr> <td></td><td> <input type='submit' value='СОХРАНИТЬ'/> </td></tr></table> </form> </details></fieldset><br/><fieldset> <details> <summary>Информация</summary> <br><span style='font-size: small; font-weight: bold'><table><tr><td>Имя хоста:</td><td align='right' id='id_local_host'></td></tr></table><hr><h5 align='left'><b>Точка доступа весов</b></h5><table><tr><td id='id_ap_ssid'></td><td align='right' id='id_ap_ip'></td></tr></table><hr><table><tr><td>MAC:</td><td align='right' id='id_mac'></td></tr></table><hr><table><tr><td>Пломба:</td><td align='right'><div id='sl_id'></div></td></tr></table></span> <hr><a href='/calibr.html'>калибровка</a></details></fieldset><hr><footer align='center'>2018 © Powered by www.scale.in.ua</footer></body></html>)";

const char global_css[] PROGMEM = R"(html{background: #ffffff;}body{font-family: Arial, sans-serif;width: 330px;height: auto;margin-right: auto;margin-left: auto;border: 5px solid #d3d3d3;background: #d3d3d3;}h1{margin: inherit;}h2{margin: auto;}h4,h5{margin: auto;}fieldset{background: #c1c1c1;border: 0px;}#brand_name{color: white;text-align: right;}img{width:auto;height:30px;}footer{font-family: Arial, sans-serif;font-size: small;}.btn, .btn:link, .btn:visited {display: inline-block;text-decoration: none;text-transform: uppercase;}.btn:hover, .btn:focus {color: white;}.btn--s {padding: 0.5em;}.btn--m {padding: 0.5em;width: 100%;padding-left: 0px;padding-right: 0px;}.btn--full, .btn--full:link {display: block;margin-left: auto;margin-right: auto;text-align: center;width: 100%;}.btn--blue:link, .btn--blue:visited {background-color: #c1c1c1;}.btn--blue:hover, .btn--blue:focus {background-color: #a0a0a0;})";

typedef struct {
	String wwwUsername;
	String wwwPassword;
} strHTTPAuth;

class CoreClass : public TaskController, AsyncWebHandler{
	protected:
		settings_t * _settings;
		strHTTPAuth _httpAuth;
		//bool _downloadSettings();
		bool _saveHTTPAuth();
		bool _downloadHTTPAuth();
		
		WiFiEventHandler StModeConnectedHandler;
		WiFiEventHandler StModeDisconnectedHandler;
	public:
		CoreClass();
		virtual ~CoreClass(){};
		void begin();
		void init();
		void initWiFi();
		void initServer();
		void connectSTA();
		void reconnectWifi(AsyncWebServerRequest * request);
		void handleScaleProp(AsyncWebServerRequest * request);
		//bool saveSettings();
		void exit();		
		void handle();
		void onSTAModeConnected(const WiFiEventStationModeConnected& evt);
		void onSTAModeDisconnected(const WiFiEventStationModeDisconnected& evt);
		bool checkAdminAuth(AsyncWebServerRequest * r);
		bool isAuthentified(AsyncWebServerRequest * request);
		String getHash(const int code, const String& date, const String& type, const String& value);
		bool eventToServer(const String& date, const String& type, const String& value);
		bool saveEvent(const String& event, const String& value);
		virtual bool canHandle(AsyncWebServerRequest *request) override final;
		virtual void handleRequest(AsyncWebServerRequest *request) override final;		
		virtual bool isRequestHandlerTrivial(){return false;};
};

class CaptiveRequestHandler : public AsyncWebHandler {
	public:
		CaptiveRequestHandler() {}
		virtual ~CaptiveRequestHandler() {}
	
		bool canHandle(AsyncWebServerRequest *request){
			if (!request->host().equalsIgnoreCase(WiFi.softAPIP().toString())){
				return true;
			}
			return false;
		}

		void handleRequest(AsyncWebServerRequest *request) {
			AsyncWebServerResponse *response = request->beginResponse(302,"text/plain","");
			response->addHeader("Location", String("http://") + WiFi.softAPIP().toString());
			request->send ( response);
		}
};

class PowerClass : public Task{	
	public:	
		PowerClass() : Task(2400000){			
			pinMode(EN_NCP, OUTPUT);
			digitalWrite(EN_NCP, HIGH);
		}
		void off(){
			digitalWrite(EN_NCP, LOW); /// Выключаем стабилизатор
			#ifdef POWER_SUPPLY_5V
				digitalWrite(PIN_ON_5V, LOW);
			#endif
		}
		void setInterval(unsigned long _interval){
			if (_interval < 2400000){
				return;	
			}
			Task::setInterval(_interval);
		}			
		
		void checkButtonPressed(){
			if(digitalRead(PWR_SW)==HIGH){
				unsigned long t = millis();
				digitalWrite(LED, HIGH);
				while(digitalRead(PWR_SW)==HIGH){ //
					delay(100);
					if(t + 4000 < millis()){ //
						digitalWrite(LED, HIGH);
						while(digitalRead(PWR_SW) == HIGH){delay(10);};//
						run();
						break;
					}
					digitalWrite(LED, !digitalRead(LED));
				}
			}
		}
};

class BlinkClass : public Task{
	private:
		unsigned int _flash = 500;		
	public:
		BlinkClass() : Task(500){
			pinMode(LED, OUTPUT);
			onRun(std::bind(&BlinkClass::blinkAP,this));	
		};
		void blinkSTA(){
			static unsigned char clk;		
			bool led = !digitalRead(LED);
			digitalWrite(LED, led);
			if (clk < 6){
				led ? _flash = 70 : _flash = 40;
				clk++;
			}else{
				_flash = 2000;
				digitalWrite(LED, HIGH);
				clk = 0;
			}
			setInterval(_flash);
		}
		void blinkAP(){
			bool led = !digitalRead(LED);
			digitalWrite(LED, led);
			setInterval(500);
		}
};

class BatteryClass : public Task{
	protected:
		unsigned int _charge;
		int * _max;
		int _get_adc(byte times = 1);
	public:
		BatteryClass(int * s, unsigned int time) : Task(time){
			_max = s;
			fetchCharge(1);	
		};
		int fetchCharge(int times){
			_charge = _get_adc(times);
			_charge = constrain(_charge, MIN_CHG, *_max);
			_charge = map(_charge, MIN_CHG, *_max, 1, 100);
			return _charge;
		};
		bool callibrated();
		void setCharge(unsigned int ch){_charge = ch;};
		unsigned int getCharge(){return _charge;};
		void setMax(int m){*_max = m;};
		int getMax(){return *_max;};
		void run(){
			fetchCharge(1);
			runned();
		};
};

class DataTimeClass : public RtcDS1307<TwoWire>{
	protected:
		uint16_t _year;
		uint8_t _month;
		uint8_t _dayOfMonth;
		uint8_t _hour;
		uint8_t _minute;
		uint8_t _second;
	public:
		DataTimeClass() : RtcDS1307<TwoWire>(Wire){};
		void setDateTime(const String& date){
			_dayOfMonth = date.substring(0, 2).toInt();
			_month = date.substring(3, 5).toInt();
			_year = date.substring(6, date.indexOf("-")).toInt();
			_hour = date.substring(11, 13).toInt();
			_minute = date.substring(14, 16).toInt();
			_second = date.substring(17).toInt();
			RtcDateTime rtc(_year, _month, _dayOfMonth, _hour, _minute, _second);
			SetDateTime(rtc);
		};	
		
		String getDateTime(){
			char datestring[20];
			RtcDateTime now = GetDateTime();
			snprintf(datestring, countof(datestring),"%04u.%02u.%02u-%02u:%02u:%02u",
				now.Year(),
				now.Month(),
				now.Day(),
				now.Hour(),
				now.Minute(),
				now.Second() 
			);
			return String(datestring);
		}	
			
};

String toStringIp(IPAddress ip);


extern AsyncWebServer server;
extern DNSServer dnsServer;
extern CoreClass * CORE;
extern Task * CONNECT;
extern PowerClass * POWER;
extern BatteryClass * BATTERY;
extern BlinkClass * BLINK;
extern IPAddress apIP;
extern IPAddress netMsk;
extern IPAddress lanIp;			// Надо сделать настройки ip адреса
extern IPAddress gateway;
extern DataTimeClass RTC;

#endif

