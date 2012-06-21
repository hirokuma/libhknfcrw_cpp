#include <iostream>
#include <cstring>
#include <unistd.h>
#include "HkNfcDep.h"

namespace {
	const char keyword1[] = "hiro99ma";
	const char keyword2[] = "HIRO99MA";
}

int nfc_test()
{
	bool b;
	uint8_t recvbuf[256];
	HkNfcDep dep;

	std::cout << "\nOpen" << std::endl;

	b = HkNfcRw::open();
	if(!b) {
		std::cout << "open fail" << std::endl;
		HkNfcRw::close();
		return -1;
	}

	std:: cout << "0:initiator  1:target : " << std::endl;
	int selnum;
	std::cin >> selnum;

	if(selnum == 0) {
		std::cout << "\nInitiator" << std::endl;
		
		b = dep.startAsInitiator(HkNfcDep::PSV_424K);
		
		if(b) {
			std::cout << "OK" << std::endl;
			dep.stopAsInitiator();
		}
	} else {
		std::cout << "\nTarget" << std::endl;

		b = dep.startAsTarget();
		
		if(b) {
			std::cout << "OK" << std::endl;
		}
	}
	std::cout << "exec = " << b << std::endl;

	std::cout << "\nClose" << std::endl;
	HkNfcRw::close();

	return 0;
}
