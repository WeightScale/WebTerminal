#include "Scales.h"

using namespace std;
using namespace std::placeholders;

void ScalesClass::init(){
	onRun(bind(&ScalesClass::fetchWeight, this));
	reset();
	_scales_value = &CoreMemory.eeprom.scales_value;
	SetFilterWeight(_scales_value->filter);
	//_downloadValue();
	mathRound();
	tare();
	SetCurrent(readAverage());
	
	webSocket.onEvent(onWsEvent);
	_server->addHandler(&webSocket);
	
	_server->on("/wt",HTTP_GET, [this](AsyncWebServerRequest * request){						/* Получить вес и заряд. */
		AsyncResponseStream *response = request->beginResponseStream("text/json");
		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.createObject();
		doData(json);
		json.printTo(*response);
		request->send(response);
		POWER->updateCache();
	});
	_server->on(CALIBR_FILE, [this](AsyncWebServerRequest * request) {							/* Открыть страницу калибровки.*/
		if(!request->authenticate(_scales_value->user, _scales_value->password))
		if (!CORE->checkAdminAuth(request)){
			return request->requestAuthentication();
		}
		saveValueCalibratedHttp(request);
	});
	_server->on("/av", [this](AsyncWebServerRequest * request){									/* Получить значение АЦП усредненное. */
		request->send(200, "text/html", String(readAverage()));
	});
	_server->on("/tp", [this](AsyncWebServerRequest * request){									/* Установить тару. */
		tare();
		request->send(204, "text/html", "");
	});
	_server->on("/sl", bind(&ScalesClass::handleSeal, this, _1));																/* Опломбировать */
	_server->on("/cdate.json",HTTP_ANY, [this](AsyncWebServerRequest * request){
		if(!request->authenticate(_scales_value->user, _scales_value->password))
			if (!CORE->checkAdminAuth(request)){
				return request->requestAuthentication();
			}
		AsyncResponseStream *response = request->beginResponseStream("application/json");
		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.createObject();
		
		json[STEP_JSON] = _scales_value->step;
		json[AVERAGE_JSON] = _scales_value->average;
		json[WEIGHT_MAX_JSON] = _scales_value->max;
		json[OFFSET_JSON] = _scales_value->offset;
		json[ACCURACY_JSON] = _scales_value->accuracy;
		json[SCALE_JSON] = _scales_value->scale;
		json[FILTER_JSON] = GetFilterWeight();
		json[SEAL_JSON] = _scales_value->seal;
		json[USER_JSON] = _scales_value->user;
		json[PASS_JSON] = _scales_value->password;
		
		json.printTo(*response);
		request->send(response);
		//reguest->send(SPIFFS, reguest->url());
	});
}

void ScalesClass::fetchWeight(){
	float w = getWeight();
	formatValue(w,_buffer);
	detectStable(w);
}

long ScalesClass::readAverage() {
	long long sum = 0;
	for (byte i = 0; i < _scales_value->average; i++) {
		sum += read();
	}
	return _scales_value->average == 0?sum / 1:sum / _scales_value->average;
}

long ScalesClass::getValue() {
	Filter(readAverage());
	return Current() - _scales_value->offset;
}

float ScalesClass::getUnits() {
	float v = getValue();
	return (v * _scales_value->scale);
}

float ScalesClass::getWeight(){
	return round(getUnits() * _round) / _round;
}

void ScalesClass::detectStable(float w){
	static long int time,time1;
	static float weight_temp;
	static unsigned char stable_num;
	if (weight_temp == w) {
		if (stable_num > STABLE_NUM_MAX) {
			if (!_stableWeight){
				if(fabs(w) > _stableStep && time > (time1 + 6000)){
					saveWeight.isSave = true;
					saveWeight.value = w;
					time1 = millis();
				}
				_stableWeight = true;
			}
			return;
		}
		stable_num++;
	} else {
		stable_num = 0;
		_stableWeight = false;
		time = millis();
	}
	weight_temp = w;
}

/*
bool ScalesClass::_downloadValue(){
	_scales_value.average = 1;
	_scales_value.step = 1;
	_scales_value.accuracy = 0;
	_scales_value.offset = 0;
	_scales_value.max = 1000;
	_scales_value.scale = 1;
	SetFilterWeight(80);
	_scales_value.user = "admin";
	_scales_value.password = "1234";
	File dateFile = SPIFFS.open(CDATE_FILE, "r");
	
	if (!dateFile) {
		return false;
	}
	size_t size = dateFile.size();
	
	unique_ptr<char[]> buf(new char[size]);
	
	dateFile.readBytes(buf.get(), size);
	dateFile.close();
	DynamicJsonBuffer jsonBuffer(size);
	JsonObject& json = jsonBuffer.parseObject(buf.get());

	if (!json.success()) {
		return false;
	}
	_scales_value.max = json[WEIGHT_MAX_JSON];
	_scales_value.offset = json[OFFSET_JSON];
	setAverage(json[AVERAGE_JSON]);
	_scales_value.step = json[STEP_JSON];
	_scales_value.accuracy = json[ACCURACY_JSON];
	_scales_value.scale = json[SCALE_JSON];
	SetFilterWeight(json[FILTER_JSON]);
	_scales_value.seal = json[SEAL_JSON];
	if (!json.containsKey(USER_JSON)){
		_scales_value.user = "admin";
		_scales_value.password = "1234";
	}else{
		_scales_value.user = json[USER_JSON].as<String>();
		_scales_value.password = json[PASS_JSON].as<String>();
	}	
	return true;	
}*/

/*
bool ScalesClass::saveDate() {	
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	
	json[STEP_JSON] = _scales_value.step;
	json[AVERAGE_JSON] = _scales_value.average;
	json[WEIGHT_MAX_JSON] = _scales_value.max;
	json[OFFSET_JSON] = _scales_value.offset;
	json[ACCURACY_JSON] = _scales_value.accuracy;
	json[SCALE_JSON] = _scales_value.scale;
	json[FILTER_JSON] = GetFilterWeight();
	json[SEAL_JSON] = _scales_value.seal;
	json[USER_JSON] = _scales_value.user;
	json[PASS_JSON] = _scales_value.password;
	
	File cdateFile = SPIFFS.open(CDATE_FILE, "w");
	if (!cdateFile) {
		return false;
	}
	
	json.printTo(cdateFile);
	cdateFile.flush();
	cdateFile.close();
	return true;
}*/

size_t ScalesClass::doData(JsonObject& json ){
	json["w"]= String(_buffer);
	json["c"]= BATTERY->getCharge();
	json["s"]= _stableWeight;
	return json.measureLength();
}

void ScalesClass::saveValueCalibratedHttp(AsyncWebServerRequest * request) {
	if (request->args() > 0) {
		if (request->hasArg("update")){
			_scales_value->accuracy = request->arg("weightAccuracy").toInt();
			_scales_value->step = request->arg("weightStep").toInt();
			setAverage(request->arg("weightAverage").toInt());
			SetFilterWeight(request->arg("weightFilter").toInt());
			_scales_value->filter = GetFilterWeight();
			_scales_value->max = request->arg("weightMax").toInt();
			mathRound();
			if (CoreMemory.save()){
				goto ok;
			}
			goto err;
		}
		
		if (request->hasArg("zero")){
			_scales_value->offset = readAverage();
		}
		
		if (request->hasArg("weightCal")){
			float rw = request->arg("weightCal").toFloat();
			long cw = readAverage();
			mathScale(rw,cw);
		}
		
		if (request->hasArg("user")){
			request->arg("user").toCharArray(_scales_value->user,request->arg("user").length()+1);
			request->arg("pass").toCharArray(_scales_value->password,request->arg("pass").length()+1);
			if (CoreMemory.save()){
				goto url;
			}
			goto err;
		}
		
		ok:
		return request->send(200, "text/html", "");
		err:
		return request->send(400, "text/html", "Ошибка ");
	}
	url:
#ifdef HTML_PROGMEM
	request->send_P(200,F("text/html"), calibr_html);
#else
	request->send(SPIFFS, request->url());
#endif
}

void ScalesClass::handleSeal(AsyncWebServerRequest * request){
	randomSeed(readAverage());
	_scales_value->seal = random(1000);
	
	if (CoreMemory.save()){
		request->send(200, "text/html", String(_scales_value->seal));
	}
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
	if(type == WS_EVT_CONNECT){
		client->ping();
	}else if(type == WS_EVT_DATA){
		String msg = "";
		for(size_t i=0; i < len; i++) {
			msg += (char) data[i];
		}
		if (msg.equals("/wt")){
			DynamicJsonBuffer jsonBuffer;
			JsonObject& json = jsonBuffer.createObject();
			size_t lengh = SCALES->doData(json);							
			AsyncWebSocketMessageBuffer * buffer = webSocket.makeBuffer(lengh);
			if (buffer) {
				json.printTo((char *)buffer->get(), lengh + 1);
				if (client) {
					client->text(buffer);
				}
			}
			POWER->updateCache();
		}
	}
}

AsyncWebSocket webSocket("/ws");
ScalesClass * SCALES;

