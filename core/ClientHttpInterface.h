
#ifndef __ClientHttpInterface__
#define __ClientHttpInterface__


#include "ThreadSafeQueue.h"

#include <stdio.h>
#include <vector>
#include <string>
#include <functional>

class TuiTable;

struct ClientHttpInterfaceThreadInput {
    std::string remoteFile;
    std::function<void(const std::string& result)> callbackFunc;
    bool exit = false;
};


struct ClientHttpInterfaceThreadOutput {
    std::string result;
    std::function<void(std::string result)> callbackFunc;
};

class ClientHttpInterface {
    
private:
    
    ThreadSafeQueue<ClientHttpInterfaceThreadInput>* inputQueue;
    ThreadSafeQueue<ClientHttpInterfaceThreadOutput>* outputQueue;
    
    std::thread* _thread;
    uint32_t updateTimerID;

	//uint8_t* readBuffer;
    
public:
	ClientHttpInterface();
    ~ClientHttpInterface();
    
    void bindTui(TuiTable* rootTable); //adds an http.get function
    void update(); //must be called periodically to process events

    void get(std::string remoteURL, std::function<void(const std::string& result)> callback = nullptr);
	//void downloadFile(std::string remoteFile, std::string localPath, std::function<void(bool success)> callback = nullptr); //WARNING! not implemented
    
private:
    
    void updateThread();
    std::string getDataInternal(std::string remoteURL);
};


#endif // !__ClientHttpInterface__
