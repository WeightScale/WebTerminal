// Scales.h

#ifndef _SCALES_h
#define _SCALES_h

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include "Task.h"
#include "Core.h"
#include "CoreMemory.h"

#define CALIBR_FILE			"/calibr.html"
#define CDATE_FILE			"/cdate.json"

#define WEIGHT_MAX_JSON		"mw_id"
#define OFFSET_JSON			"ofs"
#define AVERAGE_JSON		"av_id"
#define STEP_JSON			"st_id"
#define ACCURACY_JSON		"ac_id"
#define SCALE_JSON			"scale"
#define FILTER_JSON			"fl_id"
#define SEAL_JSON			"sl_id"
#define USER_JSON			"us_id"
#define PASS_JSON			"ps_id"

#define PIN_DOUT			16				/* Сигнал АЦП DOUT */
#define PIN_SCK				14				/* Сигнал АЦП SCK */

#define STABLE_NUM_MAX		10

//calibr.html
const char calibr_html[] PROGMEM = R"(<!DOCTYPE html><html lang='en'><head> <meta charset='UTF-8'> <meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1.0, user-scalable=no'/> <meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'/> <meta http-equiv='Pragma' content='no-cache'/> <title>Калибровка</title> <link rel='shortcut icon' href='favicon.png' type='image/png'> <link rel='stylesheet' type='text/css' href='global.css'> <style>#wt_id{display: inline; background: #fff; font-size: 28px; font-family: Arial, sans-serif; color: #618ad2; margin-left: auto; margin-right: auto;}table{width: 100%;}input{width: 100%; text-align: right; font-size: 18px; height: auto;}input[type='submit']{width: auto;}select{width: 100%; text-align-last: right; font-size: 18px; height: auto; border: 1px solid #ccc;}</style> <script>window.onload=function (){load('calibr.js', 'js', function (){GetSettings();});}; function load(e, t, n){var d=document; if ('js'===t){var a=d.createElement("script"); a.src=e, a.type='text/javascript', a.async=!1, a.onload=function (){n()}, d.getElementsByTagName('head')[0].appendChild(a)}else if ('css'===t){var a=d.createElement('link'); a.href=e, a.rel='stylesheet', a.type='text/css', a.async=!1, a.onload=function (){n()}, d.getElementsByTagName('head')[0].appendChild(a)}}</script></head><body style='visibility: hidden'><a href='/settings.html' class='btn btn--s btn--blue'>&lt;</a>&nbsp;&nbsp;<strong>Калибровка</strong><hr><fieldset id='form_max' style='visibility: visible'> <legend>Общии настройки</legend> <form id='form_c_id' action='javascript:setMax()'> <table> <tr> <td>Точность измерения</td><td> <select id='ac_id' name='weightAccuracy' title='Знак после запятой'> <option name='0' value='0'>0</option> <option name='0.0' value='1'>0.0</option> <option name='0.00' value='2'>0.00</option> <option name='0.000' value='3'>0.000</option> </select> </td></tr><tr> <td>Шаг измерения</td><td> <select id='st_id' name='weightStep' title='Дискретность измерения'> <option name='шаг 1' value='1'>1</option> <option name='шаг 2' value='2'>2</option> <option name='шаг 5' value='5'>5</option> <option name='шаг 10' value='10'>10</option> <option name='шаг 20' value='20'>20</option> <option name='шаг 50' value='50'>50</option> </select> </td></tr><tr> <td>Кол-во измерений</td><td> <select id='av_id' name='weightAverage' title='Измерение АЦП'> <option name='одно' value='1'>ОДИН</option> <option name='два' value='2'>ДВА</option> </select> </td></tr><tr> <td>Фильтр</td><td> <select id='fl_id' name='weightFilter' title='Фильтр. Меньше значение больше фильтрация'> <option name='5%' value='5'>5 %</option> <option name='10%' value='10'>10 %</option> <option name='20%' value='20'>20 %</option> <option name='30%' value='30'>30 %</option> <option name='40%' value='40'>40 %</option> <option name='50%' value='50'>50 %</option> <option name='60%' value='60'>60 %</option> <option name='70%' value='70'>70 %</option> <option name='80%' value='80'>80 %</option> <option name='90%' value='90'>90 %</option> <option name='100%' value='100'>100 %</option> </select> </td></tr><tr> <td>НВП</td><td> <input title='Максимальный вес' type='number' min='1' max='100000' id='mw_id' name='weightMax' placeholder='Найбольший предел'> </td></tr><tr> <td><a href='javascript:saveValue();'>сохранить и выйти</a></td><td> <input id='id_bs' type='submit' value='ДАЛЬШЕ >>'/> </td></tr></table> </form></fieldset><fieldset> <details> <summary>Авторизация для калибровки</summary> <form method='POST'> <table> <tr> <td>ИМЯ:</td><td> <input id='us_id' name='user' placeholder='имя админ'> </td></tr><tr> <td>ПАРОЛЬ:</td><td> <input type='password' id='ps_id' name='pass' placeholder='пароль админ'> </td></tr><tr> <td></td><td> <input type='submit' value='СОХРАНИТЬ'/> </td></tr></table> </form> </details></fieldset></body></html>)";

typedef struct{
	bool isSave;
	float value;
}t_save_value;

class ScalesClass : public Task, HX711{
	private:
		float _weight;
		char _buffer[10];
	protected:
		AsyncWebServer * _server;
		t_save_value saveWeight;
		t_scales_value * _scales_value;
		float _round;						/* множитиль для округления */
		float _stableStep;					/* шаг для стабилизации */
		bool _stableWeight;
		//bool _downloadValue();

	public:
		ScalesClass(AsyncWebServer * server) :Task(200), HX711(PIN_DOUT, PIN_SCK){
			_server = server;
			//_authenticated = false;
			saveWeight.isSave = false;
			saveWeight.value = 0.0;
			init();
		}
		void init();
		void fetchWeight();
		long readAverage();
		long getValue();
		float getUnits();
		float getWeight();
		void formatValue(float value, char* string){ 
			dtostrf(value, 6-_scales_value->accuracy, _scales_value->accuracy, string);
		};
		void mathRound(){
			_round = pow(10.0, _scales_value->accuracy) / _scales_value->step; // множитель для округления}
			_stableStep = 1/_round;
		}
		void setAverage(unsigned char a){
			_scales_value->average = constrain(a, 1, 5);
		};
		void mathScale(float referenceW, long calibrateW){
			_scales_value->scale = referenceW / float(calibrateW - _scales_value->offset);
		};
		void tare(){_scales_value->offset = readAverage();};
		void detectStable(float);
		float isSave(){return saveWeight.isSave;};
		void setIsSave(bool s){saveWeight.isSave = s;};
		float getSaveValue(){return saveWeight.value;};
		size_t doData(JsonObject& json );
		//bool saveDate();
		void saveValueCalibratedHttp(AsyncWebServerRequest * request);
		void handleSeal(AsyncWebServerRequest * request);
		int getSeal(){ return _scales_value->seal;};
		void adcOff(){power_down();};
};

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);

extern AsyncWebSocket webSocket;
extern ScalesClass * SCALES;

#endif

