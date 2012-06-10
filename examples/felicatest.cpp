#include <iostream>
#include <stdio.h>
#include "nfc.h"


void felica_test()
{
	NfcF* pNfcF = Nfc::getInstanceF();
	if(pNfcF) {
		std::cout << "\nRequest System Code" << std::endl;
		uint8_t num;
		bool b = pNfcF->reqSystemCode(&num);
		if(b) {
			std::cout << "\nSystem Code Num : " << (int)num << std::endl;
		}
		else {
			std::cout << "\nreq_sys_code fail" << std::endl;
		}

		std::cout << "\nPolling" << std::endl;
		b = NfcF::polling(0x0003);
		Nfc::debug_nfcid();

		std::cout << "\nSearch Service Code" << std::endl;
		pNfcF->searchServiceCode();
	}
}


void felica_push()
{
	std::cout << "\nPush" << std::endl;

	NfcF* pNfcF = Nfc::getInstanceF();
	if(pNfcF) {
		const char* url = "http://www.yahoo.co.jp/";
		uint8_t str_len = std::strlen(url) + 2;
		uint8_t data[256];
		uint8_t data_len;
		//
		int chksum = 0;
		int cnt = 0;

		// 個別部数
		data[cnt] = 0x01;		//個別部数
		chksum += data[cnt] & 0xff;
		cnt++;

		// 個別部ヘッダ
		data[cnt] = 0x02;		//URL
		chksum += data[cnt] & 0xff;
		cnt++;
		data[cnt] = (uint8_t)(str_len & 0x00ff);
		chksum += data[cnt] & 0xff;
		cnt++;
		data[cnt] = (uint8_t)((str_len & 0xff00) >> 8);
		chksum += data[cnt] & 0xff;
		cnt++;

		str_len -= 2;

		// 個別部パラメータ
		data[cnt] = (uint8_t)(str_len & 0x00ff);
		chksum += data[cnt] & 0xff;
		cnt++;
		data[cnt] = (uint8_t)((str_len & 0xff00) >> 8);
		chksum += data[cnt] & 0xff;
		cnt++;
		for(int i=0; i<str_len; i++) {
			data[cnt] = url[i];
			chksum += data[cnt] & 0xff;
			cnt++;
		}

		//チェックサム
		short sum = (short)-chksum;
		data[cnt] = (uint8_t)((sum & 0xff00) >> 8);
		cnt++;
		data[cnt] = (uint8_t)(sum & 0x00ff);
		cnt++;

		data_len = cnt;

		//
		bool b = pNfcF->push(data, data_len);
		if(!b) {
			std::cout << "push fail" << std::endl;
		}
	}
}


