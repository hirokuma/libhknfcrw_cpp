#include <unistd.h>
#include "misc.h"

namespace HkNfcRwMisc {


void msleep(uint16_t msec) {
	usleep(msec * 1000);
}

}	//namespace HkNfcRwMisc
