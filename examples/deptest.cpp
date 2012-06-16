#include <iostream>
#include "HkNfcDep.h"


int nfc_test()
{
	bool b;
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
		
		b = dep.startAsInitiator(HkNfcDep::ACT_106K);
	} else {
		std::cout << "\nTarget" << std::endl;

		b = dep.startAsTarget();
	}
	std::cout << "exec = " << b << std::endl;

	std::cout << "\nClose" << std::endl;
	pRw->close();

	return 0;
}
