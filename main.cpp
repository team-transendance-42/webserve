#include <iostream>

int main(int argc, char** argv) {
	if (argc == 1) {
		std::cout << "using default.conf\n";
	}
	else if (argc == 2) {
		std::cout << argv[1] << std::endl;
		// run the program: TODO
	} else {
		std::cout << "naughty naughty, you can enter the name of the .config file only\n";
	}
	return 0;
}