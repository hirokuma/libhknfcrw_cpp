#include <iostream>
#include <cstring>
#include <unistd.h>
#include <time.h>
#include "HkNfcRw.h"
#include "HkNfcLlcpI.h"
#include "HkNfcLlcpT.h"

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
		
		b = HkNfcLlcpI::start(HkNfcLlcpI::PSV_424K);
		
		if(b) {
			std::cout << "OK" << std::endl;
			
//			time_t t = time(0);
			while(HkNfcLlcpI::getDepMode() != HkNfcLlcpI::DEP_NONE) {
				HkNfcLlcpI::poll();
//				if(time(0) - t > 2) {
//					HkNfcLlcpI::stopRequest();
//				}
			}
		}
	} else {
		std::cout << "\nTarget" << std::endl;

		b = HkNfcLlcpT::start();
		
		if(b) {
			std::cout << "OK" << std::endl;
			time_t t = time(0);
			while(HkNfcLlcpT::getDepMode() != HkNfcLlcpT::DEP_NONE) {
				HkNfcLlcpT::poll();
				if(time(0) - t > 2) {
					HkNfcLlcpT::stopRequest();
				}
			}
		}
	}
	std::cout << "exec = " << b << std::endl;

	std::cout << "\nClose" << std::endl;
	HkNfcRw::close();

	return 0;
}
