#include <iostream>
#include <cstdio>
#include <cstring>

#include "HkNfcRw.h"

int nfc_test()
{
	bool b;
	HkNfcRw* pRw = HkNfcRw::getInstance();

	std::cout << "\nOpen" << std::endl;

	b = pRw->open();
	if(!b) {
		std::cout << "open fail" << std::endl;
		pRw->close();
		return -1;
	}
	std::cout << "\nCard Detect & read" << std::endl;

	HkNfcF nfcf(pRw);
	HkNfcRw::Type type = pRw->detect(0, 0, &nfcf);
	if(type != HkNfcRw::NFC_F) {
		std::cout << "detect fail" << std::endl;
		pRw->close();
		return -1;
	}

	std::cout << "\nread" << std::endl;
	b = nfcf.polling(0x12fc);
	if(b) {
		std::cout << "NFC-F card." << std::endl;
	} else {
		std::cout << "not NFC-F card." << std::endl;

		b = nfcf.polling();
		if(!b) {
			std::cout << "not FeliCa card." << std::endl;
			pRw->close();
			return -1;
		}
		std::cout << "FeliCa card." << std::endl;
	}

	uint8_t buf[16];

	for(int blk=0; blk<14; blk++) {
		bool ret = nfcf.read(buf, blk);
		if(ret) {
			for(int i=0; i<16; i++) {
                printf("%02x ", buf[i]);
			}
            printf("\n");
		} else {
			std::cout << "fail : " << blk << std::endl;
			break;
		}
	}
	std::cout << "----------------------------" << std::endl;

	nfcf.setServiceCode(HkNfcF::SVCCODE_RO);
	for(int blk=0x80; blk<=0x88; blk++) {
		bool ret = nfcf.read(buf, blk);
		if(ret) {
			for(int i=0; i<16; i++) {
                printf("%02x ", buf[i]);
			}
            printf("\n");
		} else {
			std::cout << "fail : " << blk << std::endl;
			break;
		}
	}

	std::cout << "\nClose" << std::endl;

	pRw->close();

	return 0;
}
