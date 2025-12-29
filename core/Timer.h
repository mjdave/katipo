
#ifndef __Timer__
#define __Timer__

#include <stdio.h>
#include <iostream>
#include <chrono>
#include <ctime>

class Timer {
	std::chrono::steady_clock::time_point lastTime;
	std::chrono::steady_clock::time_point startTime;
    
public:
    Timer();
    ~Timer();
    
    double getDt();
	double getElapsed();
};

#endif // !__Timer__
