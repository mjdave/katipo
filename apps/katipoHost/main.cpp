#include <iostream>
#include "Controller.h"

int main(int argc, const char * argv[]) {
    Controller::getInstance()->init(argc, argv);
    return 0;
}
