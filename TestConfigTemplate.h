//
// Created by Francesco Laurita on 6/2/16.
//

#ifndef SPEEDTEST_TESTCONFIGTEMPLATE_H
#define SPEEDTEST_TESTCONFIGTEMPLATE_H
#include "SpeedTest.h"

//                                         start_size   max_size   inc_size  buff_size  min_test_time_ms   concurrency
const TestConfig preflightConfigDownload = {   600000,   2000000,    125000,      4096,     10000,         2, "Preflight check"};

const TestConfig slowConfigDownload      = {   100000,   5000000,    100000,      4096,     20000,         2, "Very-slow-line line type detected: profile selected slowband"};
const TestConfig narrowConfigDownload    = {  1000000, 100000000,    500000,     16384,     20000,         4, "Buffering-lover line type detected: profile selected narrowband"};
const TestConfig broadbandConfigDownload = {  2500000, 100000000,    750000,     65536,     20000,        16, "Broadband line type detected: profile selected broadband"};
const TestConfig fiberConfigDownload     = {  5000000, 100000000,   1000000,    131072,     20000,        32, "Fiber / Lan line type detected: profile selected fiber"};

const TestConfig slowConfigUpload        = {    50000,   3500000,     50000,      4096,     20000,         2, "Very-slow-line line type detected: profile selected slowband"};
const TestConfig narrowConfigUpload      = {   500000,  70000000,    250000,     16384,     20000,         4, "Buffering-lover line type detected: profile selected narrowband"};
const TestConfig broadbandConfigUpload   = {  1250000,  70000000,    375000,     65536,     20000,         8, "Broadband line type detected: profile selected broadband"};
const TestConfig fiberConfigUpload       = {  2500000,  70000000,    500000,    131072,     20000,        16, "Fiber / Lan line type detected: profile selected fiber"};

void testConfigSelector(const double preSpeed, TestConfig& uploadConfig, TestConfig& downloadConfig) {
	uploadConfig   = slowConfigUpload;
	downloadConfig = slowConfigDownload;

	if (preSpeed > 4 && preSpeed <= 30) {
		downloadConfig = narrowConfigDownload;
		uploadConfig   = narrowConfigUpload;
	} else if (preSpeed > 30 && preSpeed < 150) {
		downloadConfig = broadbandConfigDownload;
		uploadConfig   = broadbandConfigUpload;
	} else if (preSpeed >= 150) {
		downloadConfig = fiberConfigDownload;
		uploadConfig   = fiberConfigUpload;
	}
}
#endif // SPEEDTEST_TESTCONFIGTEMPLATE_H