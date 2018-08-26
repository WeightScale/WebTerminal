// 
// 
// 

#include "CoreMemory.h"

void CoreMemoryClass::init(){
	
	begin(sizeof(MyEEPROMStruct));
	if (percentUsed() >= 0) {
		get(0, eeprom);
	}	
}

bool CoreMemoryClass::save(){
	put(0, eeprom);
	return commit();
}


CoreMemoryClass CoreMemory;

