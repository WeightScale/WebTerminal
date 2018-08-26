/*
 * WebTerminal.cpp
 *
 * Created: 20.05.2018 10:02:25
 *  Author: Kostya
 */ 
//#include "Arduino.h"
#include "Core.h"

//
//
void setup(){
	CORE = new CoreClass();
	CORE->begin();
}

void loop(){	
	CORE->handle();
}



