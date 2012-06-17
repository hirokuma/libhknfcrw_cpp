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
	HkNfcRw* pRw = HkNfcRw::getInstance();
	HkNfcDep dep(pRw);

	std::cout << "\nOpen" << std::endl;

	b = pRw->open();
	if(!b) {
		std::cout << "open fail" << std::endl;
		pRw->close();
		return -1;
	}

	std:: cout << "0:initiator  1:target : " << std::endl;
	int selnum;
	std::cin >> selnum;

	if(selnum == 0) {
		std::cout << "\nInitiator" << std::endl;
		
		b = dep.startAsInitiator(HkNfcDep::PSV_424K);
		
		if(b) {
			sleep(1);
			
			uint8_t res_size = 0;
			b = dep.sendAsInitiator(keyword1, std::strlen(keyword1), recvbuf, &res_size);
			if(b) {
				std::cout << "send keyword1" << std::endl;
				if(std::memcmp(keyword2, recvbuf, res_size) == 0) {
					std::cout << "receive keyword2" << std::endl;
				} else {
					b = false;
				}
			}
			b = dep.sendAsInitiator(keyword2, std::strlen(keyword2), recvbuf, &res_size);
			dep.stopAsInitiator();
		}
	} else {
		std::cout << "\nTarget" << std::endl;

		b = dep.startAsTarget();
		
		if(b) {
			uint8_t res_size = 0;
			b = dep.recvAsTarget(recvbuf, &res_size);
			if(std::memcmp(keyword1, recvbuf, res_size) == 0) {
				std::cout << "receive keyword1" << std::endl;
				b = dep.respAsTarget(keyword2, std::strlen(keyword2));
				std::cout << "return keyword2" << std::endl;
			} else {
				b = false;
			}
			if(b) {
				uint8_t res_size = 0;
				b = dep.recvAsTarget(recvbuf, &res_size);
				if(std::memcmp(keyword2, recvbuf, res_size) == 0) {
					std::cout << "receive keyword2" << std::endl;
					b = dep.respAsTarget(0, 0);
				}
			}
			while(b) {
				b = dep.recvAsTarget(recvbuf, &res_size);
			}
		}
	}
	std::cout << "exec = " << b << std::endl;

	std::cout << "\nClose" << std::endl;
	pRw->close();

	return 0;
}
