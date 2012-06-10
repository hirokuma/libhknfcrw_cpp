#include <iostream>
#include "nfc.h"


int main()
{
	std::cout << "\nOpen" << std::endl;
	bool bOpen = Nfc::open();
	if(!bOpen) {
		std::cout << "open fail" << std::endl;
		Nfc::close();
		return -1;
	}

	std::cout << "\nFALP" << std::endl;

	bool ret = Nfc::detect();
	if(!ret) {
		std::cout << "\nFail Close" << std::endl;
		Nfc::close();
		return 0;
	}

	NfcF* pFelica = Nfc::getInstanceF();
	const char SEND[] =
		"abcdefghijklmnopqrstuvwxyz" \
		"abcdefghijklmnopqrstuvwxyz" \
		"abcdefghijklmnopqrstuvwxyz" \
		"abcdefghijklmnopqrstuvwxyz" \
		"abcdefghijklmnopqrstuvwxyz" \
		"abcdefghijklmnopqrstuvwxyz";
	ret = pFelica->falpSendString((const uint8_t*)SEND, strlen(SEND));
	std::cout << "FALP init:" << ret << std::endl;

	std::cout << "\nClose" << std::endl;
	Nfc::close();

	return 0;
}
