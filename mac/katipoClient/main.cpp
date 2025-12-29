#include <iostream>
#include "Controller.h"
#include "TuiStringUtils.h"

#define KATIPO_VERSION "0.1"


void printErrorForMissingArgForOption(int option)
{
    switch (option) {
    case 'p':
    {
        printf("--port must be followed by a port number.\n");
    }
    break;

    default:
        break;
    }
}

void printUsage()
{
    std::string usageString = Tui::string_format("\n\
Version:%s\n\
\n\
Available options:\n\
\t--port PORT (-p) the UDP port.\n\
\t--help (-h) display this help text\n\n\
", KATIPO_VERSION);

    printf("%s\n", usageString.c_str());
}

int main(int argc, const char * argv[]) {
    // insert code here...
    
    int option_index = 0;
    int opt = 0;

    std::string port;

    /*if(argc < 2)
    {
        printUsage();
        return 0;
    }*/

    for(int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "-h" || arg == "--help")
        {
            printUsage();
            return 0;
        }
        else if(arg == "-p" || arg == "--port")
        {
            if(argc > i)
            {
                port = argv[i + 1];
                i++;
            }
        }
        else
        {
            printf("Server doesn't understand:%s\n", arg.c_str());
            return 0;
        }
    }
    
    Controller::getInstance()->init(argv[0]);
    
    
    return 0;
}
