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

#if 0
	std:: cout << "0:initiator  1:target : " << std::endl;
	int selnum;
	std::cin >> selnum;
#else
	//PaSoRiはイニシエータの方がいいらしい
	int selnum = 0;
#endif

	if(selnum == 0) {
		std::cout << "\nInitiator" << std::endl;

		Nfc* pNfc = Nfc::getInstance();
		pNfc->jumpForDep();
	} else {
		std::cout << "\nTarget" << std::endl;

		Nfc* pNfc = Nfc::getInstance();
		pNfc->initAsTarget();
	}

	std::cout << "\nClose" << std::endl;
	Nfc::close();

	return 0;
}
