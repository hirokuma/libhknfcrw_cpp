#include <iostream>
#include <cstdio>
#include <cstring>

#include "HkNfcRw.h"
#include "HkNfcF.h"

int nfc_test()
{
	bool b;

	std::cout << "\nOpen" << std::endl;

	b = HkNfcRw::open();
	if(!b) {
		std::cout << "open fail" << std::endl;
		HkNfcRw::close();
		return -1;
	}
	std::cout << "\nCard Detect & read" << std::endl;

	HkNfcRw::Type type = HkNfcRw::detect(true, true, true);
	std::cout << "type = " << (int)type << std::endl;

	sleep(1);

	type = HkNfcRw::detect(true, true, true);
	std::cout << "type = " << (int)type << std::endl;



	if(type != HkNfcRw::NFC_F) {
		std::cout << "detect fail" << std::endl;
		HkNfcRw::close();
		return -1;
	}

	std::cout << "\nread" << std::endl;
	b = HkNfcF::polling(0x12fc);
	if(b) {
		std::cout << "NFC-F card." << std::endl;
	} else {
		std::cout << "not NFC-F card." << std::endl;

		b = HkNfcF::polling();
		if(!b) {
			std::cout << "not FeliCa card." << std::endl;
			HkNfcRw::close();
			return -1;
		}
		std::cout << "FeliCa card." << std::endl;
	}

	uint8_t buf[16];

	for(int blk=0; blk<14; blk++) {
		bool ret = HkNfcF::read(buf, blk);
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

	HkNfcF::setServiceCode(HkNfcF::SVCCODE_RO);
	for(int blk=0x80; blk<=0x88; blk++) {
		bool ret = HkNfcF::read(buf, blk);
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

	HkNfcRw::close();

	return 0;
}
