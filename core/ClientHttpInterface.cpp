

#include "ClientHttpInterface.h"

#include <iostream>
#include "curl/curl.h"
#include <string>
#include "MJLog.h"
#include "TuiScript.h"

size_t writeFunction(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

ClientHttpInterface::ClientHttpInterface()
{
    inputQueue = new ThreadSafeQueue<ClientHttpInterfaceThreadInput>();
    outputQueue = new ThreadSafeQueue<ClientHttpInterfaceThreadOutput>();
    
    _thread = new std::thread(&ClientHttpInterface::updateThread, this);
}

ClientHttpInterface::~ClientHttpInterface()
{
    //MJTimer::getInstance()->removeTimer(updateTimerID);
    ClientHttpInterfaceThreadInput input;
    input.exit = true;
    inputQueue->push(input);
    
    _thread->join();
    delete _thread;
    
    delete inputQueue;
    delete outputQueue;
    
}

void ClientHttpInterface::bindTui(TuiTable* rootTable) //adds an http.get function
{
    //http.get(url)
    TuiTable* httpTable = new TuiTable(rootTable);
    rootTable->set("http", httpTable);
    httpTable->release();
    
    httpTable->setFunction("get", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        
        if(args && args->arrayObjects.size() > 1 && args->arrayObjects[0]->type() == Tui_ref_type_STRING && args->arrayObjects[1]->type() == Tui_ref_type_FUNCTION)
        {
            TuiFunction* callbackFunc = (TuiFunction*)(args->arrayObjects[1]->retain());
            get(((TuiString*)args->arrayObjects[0])->value, [callbackFunc](const std::string& result){
                TuiString* reponseData = new TuiString(result);
                callbackFunc->call("http.get response", reponseData);
                callbackFunc->release();
                reponseData->release();
            });
        }
        else
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "http.get expected url and callback function");
        }
        
        return TUI_NIL;
    });
    
}


void ClientHttpInterface::updateThread()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    while(1)
    {
        ClientHttpInterfaceThreadInput input;
        inputQueue->pop(input);

        if(input.exit)
        {
            curl_global_cleanup();
            return;
        }
        
        if(input.callbackFunc)
        {
            ClientHttpInterfaceThreadOutput output;
            output.callbackFunc = input.callbackFunc;
            output.result = getDataInternal(input.remoteFile);
            
            outputQueue->push(output);
        }
        else
        {
            getDataInternal(input.remoteFile);
        }
    }
}


std::string ClientHttpInterface::getDataInternal(std::string remoteURL)
{
    int retryCount = 0;
    bool success = false;
    std::string response_string = "";
    
    const char* header = "Cache-Control: no-cache, no-store, must-revalidate";
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, header);

    int port = 80;
    
    if(remoteURL.find("https://") == 0)
    {
        port = 443;
    }
    
    while(!success && retryCount < 2)
    {
        auto curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, remoteURL.c_str());
            curl_easy_setopt(curl, CURLOPT_PORT, port);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
            //curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "katipo/0.1");
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
            char errbuf[CURL_ERROR_SIZE];
            errbuf[0] = 0;
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
            
            //std::string header_string;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
            //curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);
            
            CURLcode res = curl_easy_perform(curl);
            
            if(res != CURLE_OK)
            {
                MJLog("curl failed with code:%d error:%s", res, errbuf);
                response_string = "";
            }
            else
            {
                success = true;
            }
            
            
            curl_easy_cleanup(curl);
        }
        
        retryCount++;
        
        if(!success)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20 * (retryCount + 1)));
        }
    }
    
    return response_string;
}


void ClientHttpInterface::update() //called from the main thread
{
    while(!outputQueue->empty())
    {
        ClientHttpInterfaceThreadOutput output;
        outputQueue->pop(output);
        output.callbackFunc(output.result);
    }
}


void ClientHttpInterface::get(std::string remoteFile, std::function<void(const std::string& result)> callback)
{
    ClientHttpInterfaceThreadInput input;
    input.remoteFile = remoteFile;
    input.callbackFunc = callback;
    inputQueue->push(input);
}

/*void ClientHttpInterface::downloadFile(std::string remoteFile, std::string localPath, std::function<void(bool success)> callback)
{
    MJError("unimplimented");
	bool success = false;
	char errorBuffer[512];
	struct mg_connection* conn = mg_download(host.c_str(), atoi(port.c_str()), 0, &errorBuffer[0], 512, "GET /%s HTTP/1.0\r\n \r\n\r\n", remoteFile.c_str() );
	if(conn)
	{
		const mg_response_info* responseInfo = mg_get_response_info(conn);

		if(responseInfo->status_code != 200)
		{
			MJLog("Failed to download file, got response:%d %s", responseInfo->status_code, responseInfo->status_text);
			mg_close_connection(conn);
			return false;
		}
		else
		{
			std::ofstream ofs(convertUtf8ToWide(localPath), std::ios::binary | std::ios::out | std::ios::trunc);

			if(!ofs.is_open())
			{
                MJLog("Failed to download file, file save location not found:%s", localPath.c_str());
				mg_close_connection(conn);
				return false;
			}

			int readBytesCount = mg_read(conn, readBuffer, HTTP_CLIENT_READ_BUFFER_SIZE);
			while(readBytesCount > 0)
			{
				ofs.write((const char*)readBuffer, readBytesCount);
				MJLog("receieved %d bytes", readBytesCount);
				readBytesCount = mg_read(conn, readBuffer, HTTP_CLIENT_READ_BUFFER_SIZE);
			}

			if(readBytesCount >= 0)
			{
				success = true;
			}
			ofs.close();
		}
	}
	else
	{
		MJLog("Failed to connect to http server at host:%s:%s. %s", host.c_str(), port.c_str(), &errorBuffer[0]);
	}


	if(conn)
	{
		mg_close_connection(conn);
	}

	return success;
    
}*/
