#include <iostream>
#include <cstdio>
#include <cstring>

#include "HkNfcRw.h"
#include "HkNfcF.h"

int nfc_test()
{
	HkNfcRw::open();
	HkNfcRw::close();

	return 0;
}
