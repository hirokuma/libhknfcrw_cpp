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
		
		b = HkNfcDep::startAsInitiator(HkNfcDep::PSV_424K, false);
		
		if(b) {
			sleep(1);
			
			uint8_t res_size = 0;
			b = HkNfcDep::sendAsInitiator(keyword1, std::strlen(keyword1), recvbuf, &res_size);
			if(b) {
				std::cout << "send keyword1" << std::endl;
				if(std::memcmp(keyword2, recvbuf, res_size) == 0) {
					std::cout << "receive keyword2" << std::endl;
				} else {
					b = false;
				}
			}
			b = HkNfcDep::sendAsInitiator(keyword2, std::strlen(keyword2), recvbuf, &res_size);
			HkNfcDep::stopAsInitiator();
		}
	} else {
		std::cout << "\nTarget" << std::endl;

		b = HkNfcDep::startAsTarget(false);
		
		if(b) {
			uint8_t res_size = 0;
			b = HkNfcDep::recvAsTarget(recvbuf, &res_size);
			if(std::memcmp(keyword1, recvbuf, res_size) == 0) {
				std::cout << "receive keyword1" << std::endl;
				b = HkNfcDep::respAsTarget(keyword2, std::strlen(keyword2));
				std::cout << "return keyword2" << std::endl;
			} else {
				b = false;
			}
			if(b) {
				uint8_t res_size = 0;
				b = HkNfcDep::recvAsTarget(recvbuf, &res_size);
				if(std::memcmp(keyword2, recvbuf, res_size) == 0) {
					std::cout << "receive keyword2" << std::endl;
					b = HkNfcDep::respAsTarget(0, 0);
				}
			}
			while(b) {
				b = HkNfcDep::recvAsTarget(recvbuf, &res_size);
			}
		}
	}
	std::cout << "exec = " << b << std::endl;

	std::cout << "\nClose" << std::endl;
	HkNfcRw::close();

	return 0;
}
