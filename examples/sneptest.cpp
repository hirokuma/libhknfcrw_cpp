#include <cstdio>
#include <cstring>
#include "HkNfcRw.h"
#include "HkNfcSnep.h"
#include "HkNfcNdef.h"


int nfc_test()
{
	bool b;
	HkNfcNdefMsg msg;

	std::printf("open\n");

	b = HkNfcRw::open();
	if(!b) {
		std::printf("open fail\n");
		HkNfcRw::close();
		return -1;
	}

	b = HkNfcNdef::createText(&msg, "hiro99ma", std::strlen("hiro99ma"));
	if(!b) {
		std::printf("text fail\n");
		HkNfcRw::close();
		return -1;
	}

	b = HkNfcSnep::putStart(HkNfcSnep::MD_TARGET, &msg);
	if(!b) {
		std::printf("snep start fail\n");
		HkNfcRw::close();
		return -1;
	}

	while(HkNfcSnep::poll()) {
		;
	}
	if(HkNfcSnep::getResult() == HkNfcSnep::SUCCESS) {
		std::printf("snep success\n");
	} else {
		std::printf("snep fail\n");
	}

	HkNfcRw::close();


	return 0;
}
