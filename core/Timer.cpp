
#include "Timer.h"


Timer::Timer()
{
    startTime = lastTime = std::chrono::steady_clock::now();
}

Timer::~Timer()
{
    
}

double Timer::getDt()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::duration<double> dt = now-lastTime;
    lastTime = now;
    
    return dt.count();
}

double Timer::getElapsed()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::chrono::duration<double> dt = now-startTime;

	return dt.count();
}
