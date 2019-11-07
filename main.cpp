#include <iostream>
#include <map>
#include <iomanip>
#include "SpeedTest.h"
#include "TestConfigTemplate.h"
#include "CmdOptions.h"
#include <csignal>

void banner() {
	std::cout << "SpeedTest++ version " << SpeedTest_VERSION_MAJOR << "." << SpeedTest_VERSION_MINOR << std::endl;
	std::cout << "Speedtest.net command line interface" << std::endl;
	std::cout << "Info: " << SpeedTest_HOME_PAGE << std::endl;
	std::cout << "Author: " << SpeedTest_AUTHOR << std::endl;
}

void usage(const char* name) {
	std::cerr << "Usage: " << name << " ";
	std::cerr << "  [--latency] [--download] [--upload] [--share] [--help]\n"
	             "       [--serverid id] [--test-server host:port] [--output verbose|text]\n";
	std::cerr << "optional arguments:" << std::endl;
	std::cerr << "  --help                   Show this message and exit\n";
	std::cerr << "  --latency                Perform latency test only\n";
	std::cerr << "  --download               Perform download test only. It includes latency test\n";
	std::cerr << "  --upload                 Perform upload test only. It includes latency test\n";
	std::cerr << "  --share                  Generate and provide a URL to the speedtest.net share results image\n";
	std::cerr << "  --test-server host:port  Run speed test against a specific server\n";
	std::cerr << "  --serverid id            Run speed test against a specific ServerId\n";
	std::cerr << "  --output verbose|text    Set output type. Default: verbose\n";
}

int main(const int argc, const char **argv) {
	ProgramOptions programOptions;
	if (!ParseOptions(argc, argv, programOptions)) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (programOptions.output_type == OutputType::verbose) {
		banner();
		std::cout << std::endl;
	}

	if (programOptions.help) {
		usage(argv[0]);
		return EXIT_SUCCESS;
	}

	signal(SIGPIPE, SIG_IGN);
	auto sp = SpeedTest(SPEED_TEST_MIN_SERVER_VERSION);

	IPInfo info;
	if (!sp.ipInfo(info)) {
		std::cerr << "Unable to retrieve your IP info. Try again later" << std::endl;
		return EXIT_FAILURE;
	}
	if (programOptions.output_type == OutputType::verbose) {
		std::cout << "IP: " << info.ip_address << " (" << info.isp << ") " << "Location: [" << info.lat << ", " << info.lon << "]" << std::flush;
	} else {
		std::cout << info.ip_address << ",";
		std::cout << info.lat << ",";
		std::cout << info.lon << ",";
		std::cout << info.isp << ",";
	}

	ServerInfo serverInfo;
	auto serverList = sp.serverList();
	if (serverList.empty()) {
		std::cerr << "Unable to download server list. Try again later" << std::endl;
		return EXIT_FAILURE;
	}
	if (programOptions.selected_server.empty() && programOptions.selected_serverid == -1) {
		if (programOptions.output_type == OutputType::verbose) {
			std::cout << std::endl;
			std::cout << "Finding fastest server (" << serverList.size() << " servers online) " << std::flush;
		}
		serverInfo = sp.bestServer(10, [&programOptions](bool success) {
			if (programOptions.output_type == OutputType::verbose)
				std::cout << (success ? '.' : '*') << std::flush;
		});
	} else {
		serverInfo.host.append(programOptions.selected_server);
		for (auto &s : serverList) {
			if ( (programOptions.selected_serverid != -1 && s.id == programOptions.selected_serverid) || s.host == serverInfo.host ) {
				serverInfo = s;
				break;
			}
		}
		sp.setServer(serverInfo);
	}
	if (serverInfo.host.empty()) {
		std::cerr << "Host name is empty." << std::endl;
		return EXIT_FAILURE;
	}
	if (programOptions.output_type == OutputType::verbose) {
		std::cout << std::endl;
		std::cout << "Server: " << serverInfo.name << " " << serverInfo.host << " by " << serverInfo.sponsor << " (" << serverInfo.distance << " km from you): " << sp.latency() << " ms" << std::flush;
	} else {
		std::cout << serverInfo.id << ",";
		std::cout << serverInfo.sponsor << ",";
		std::cout << serverInfo.distance << ",";
	}

	if (programOptions.output_type == OutputType::verbose) {
		std::cout << std::endl;
		std::cout << "Ping: " << sp.latency() << " ms." << std::flush;
	} else {
		std::cout << sp.latency() << ",";
	}
	long jitter = 0;
	if (programOptions.output_type == OutputType::verbose) {
		std::cout << std::endl;
		std::cout << "Jitter: " << std::flush;
	}
	if (sp.jitter(serverInfo, jitter)) {
		if (programOptions.output_type == OutputType::verbose)
			std::cout << jitter << " ms." << std::flush;
		else
			std::cout << jitter << ",";
	} else {
		std::cerr << "Jitter measurement is unavailable at this time." << std::endl;
		return EXIT_FAILURE;
	}
	if (programOptions.latency) {
		std::cout << std::endl;
		return EXIT_SUCCESS;
	}

	if (programOptions.output_type == OutputType::verbose) {
		std::cout << std::endl;
		std::cout << "Determine line type (" << preflightConfigDownload.concurrency << ") " << std::flush;
	}
	double preSpeed = 0;
	if (!sp.downloadSpeed(serverInfo, preflightConfigDownload, preSpeed, [&programOptions](bool success) {
		if (programOptions.output_type == OutputType::verbose)
			std::cout << (success ? '.' : '*') << std::flush;
	})) {
		std::cerr << "Pre-flight check failed." << std::endl;
		return EXIT_FAILURE;
	}

	TestConfig uploadConfig;
	TestConfig downloadConfig;
	testConfigSelector(preSpeed, uploadConfig, downloadConfig);
	if (programOptions.output_type == OutputType::verbose) {
		std::cout << std::endl;
		std::cout << downloadConfig.label << std::flush;
	}

	if (!programOptions.upload) {
		if (programOptions.output_type == OutputType::verbose) {
			std::cout << std::endl;
			std::cout << "Testing download speed (" << downloadConfig.concurrency << ") " << std::flush;
		}
		double downloadSpeed = 0;
		if (sp.downloadSpeed(serverInfo, downloadConfig, downloadSpeed, [&programOptions](bool success) {
			if (programOptions.output_type == OutputType::verbose)
				std::cout << (success ? '.' : '*') << std::flush;
		})) {
			if (programOptions.output_type == OutputType::verbose) {
				std::cout << std::endl;
				std::cout << "Download: ";
				std::cout << std::fixed;
				std::cout << std::setprecision(2);
				std::cout << downloadSpeed << " Mbit/s" << std::flush;
			} else {
				std::cout << std::fixed;
				std::cout << std::setprecision(2);
				std::cout << downloadSpeed << ",";
			}
		} else {
			std::cerr << "Download test failed." << std::endl;
			return EXIT_FAILURE;
		}
	}
	if (programOptions.download) {
		std::cout << std::endl;
		return EXIT_SUCCESS;
	}

	if (programOptions.output_type == OutputType::verbose) {
		std::cout << std::endl;
		std::cout << "Testing upload speed (" << uploadConfig.concurrency << ") " << std::flush;
	}
	double uploadSpeed = 0;
	if (sp.uploadSpeed(serverInfo, uploadConfig, uploadSpeed, [&programOptions](bool success) {
		if (programOptions.output_type == OutputType::verbose)
			std::cout << (success ? '.' : '*') << std::flush;
	})) {
		if (programOptions.output_type == OutputType::verbose) {
			std::cout << std::endl;
			std::cout << "Upload: ";
			std::cout << std::fixed;
			std::cout << std::setprecision(2);
			std::cout << uploadSpeed << " Mbit/s" << std::flush;
		} else {
			std::cout << std::fixed;
			std::cout << std::setprecision(2);
			std::cout << uploadSpeed << ",";
		}
	} else {
		std::cerr << "Upload test failed." << std::endl;
		return EXIT_FAILURE;
	}

	if (programOptions.share) {
		std::string share_it;
		if (sp.share(serverInfo, share_it)) {
			if (programOptions.output_type == OutputType::verbose) {
				std::cout << std::endl;
				std::cout << "Results image: " << share_it << std::flush;
			} else {
				std::cout << share_it << std::flush;
			}
		}
	}

	std::cout << std::endl;
	return EXIT_SUCCESS;
}