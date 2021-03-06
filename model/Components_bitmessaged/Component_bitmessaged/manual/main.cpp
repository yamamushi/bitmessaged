#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <signal.h>     /* signal, raise, sig_atomic_t */
#include <set>
#include <string>
#include <map>
#include <arpa/inet.h> 
#include <getopt.h>
#include "string.h"
#include "knowledge.h"
#include "ACF.h"
#include "OutSocketHandler.h"
#include "OutConnectionHandler.h"
#include "InConnectionHandler.h"
#include "InSocketHandler.h"
#include "Trace2UML.h"
#include "version.h"

volatile bool keepRunning = true;

void my_handler (int param)
{
  keepRunning = false;
}

void print_usage()
{
    printf("\nbitmessaged: a bitmessage daemon Copyright (C) Thomas Spitzer\n");
    printf("Usage: nbitmessaged [-p <plugin>]\n");
    printf("\nOptions:\n");
    printf("--help              Display this information\n");
    printf("-d, --debug         the socket dialog is dumped to console.\n");
    printf("-p, --plugin        plugin to load.\n");
    printf("-i, --initial       ip:port of the initial client connection.\n");
    printf("-n, --portnum       the port number for accepting connections (default 8444).\n");
    printf("                    (you may have multiple addresses).\n");
    printf("-f, --fixed         fixed to node addresses in database. Don't add nodes.\n");

    printf("\nTuning parapeters:\n");
    printf("-o, --outgoing      number of outgoing connections (default 8).\n");
    printf("-a, --accept        number of incomming connections (default 16).\n");
}

ACF soketContext;
ACF logicContext;

pthread_t socketThread;
pthread_t logicThread;

std::set<std::string> plugins;
std::map<std::string, void*> plugin_handles;
static data::knowledge database;

void start_plugin(std::string filename)
{
    printf("Loading plugin %s\n",filename.c_str());
    void* handle = dlopen (filename.c_str(), RTLD_LAZY);
    char* anError = dlerror();
    if (anError)
    {
        printf("%s\n",anError);
    }
    else
    {
        plugin_handles[filename] = handle;
        void(* func)(data::knowledge&) = (void(*)(data::knowledge&)) dlsym (handle, "init_plugin");
        func(database);
    }
}

void stop_plugin(std::string filename)
{
    if (plugin_handles.find(filename) == plugin_handles.end())
        return;
    printf("shutting down plugin %s\n",filename.c_str());
    void(* func)() = (void(*)()) dlsym (plugin_handles[filename], "shutdown_plugin");
    func();
}

extern void* do_fastcgi(void*);

int main(int argc, char *argv[]) {

    protocol::Payload::initSemaphore();

    Trace2UML::ms_ofile.open("Trace2UML.seq");
    static struct option long_options[] = {
        {"help"       ,  no_argument,        0,  'h' },
        {"debug"      ,  no_argument,        0,  'd' },
        {"plugin"     ,  optional_argument,  0,  'p' },
        {"outgoing"   ,  optional_argument,  0,  'o' },
        {"accept"     ,  optional_argument,  0,  'a' },
        {"initial"    ,  required_argument,  0,  'i' },
        {"portnum"    ,  required_argument,  0,  'n' },
        {"fixed"      ,  no_argument,        0,  'f' },
        {0,          0,                      0,   0  }
    };

    int long_index =0;
    int opt;
    unsigned int portnum = 8444;
    
    while ((opt = getopt_long(argc, argv, "n:o:a:p:i:hdf", long_options, &long_index )) != -1) {
        switch (opt) {
        case 'n':
                portnum = atoi(optarg);
            break;
        case 'o':
                OutConnectionHandler::setOutputCount(atoi(optarg));
            break;
        case 'a':
                InConnectionHandler::setInputCount(atoi(optarg));
            break;
        case 'd':
                database.setDebug();
            break;
        case 'f':
                database.setFixedMode();
            break;
        case 'p':
                plugins.insert(optarg);
            break;
        case 'h':
            print_usage();
            exit(EXIT_SUCCESS);
            break;
        case 'i':
            if (!database.addNode(optarg))
            {
                printf("ERROR: -i <addr>:<port> e.g.: -i 127.0.0.1:8444\n");
                exit (EXIT_FAILURE);
            }
            break;
        default:
            break;
        }
    }
    
    protocol::version::setListenPort(portnum);
    database.load();
    
    for (std::set<std::string>::iterator it = plugins.begin(); it != plugins.end(); it++)
    {
        start_plugin(*it);
    }

    signal (SIGINT, my_handler); // install a signal handler
    signal(SIGPIPE, SIG_IGN); // ignore sigpipe
     
    pthread_create(&socketThread,0,&ACF::staticExec,&soketContext);
    pthread_create(&logicThread,0,&ACF::staticExec,&logicContext);

    OutConnectionHandler OutHandler(&logicContext, &soketContext, database);
    InConnectionHandler InHandler((int)portnum, &logicContext, &soketContext, database);
    
    database.toOutConnectionHandler = OutHandler.MessageReceiver();
    database.toInConnectionHandler = InHandler.MessageReceiver();
    OutHandler.Initialize(0);
    InHandler.Initialize(0);
    
    
    int count = 0;
    while (keepRunning)
    {
        sleep(1);
        count++;
        if (count > 3600)
        {
            database.save();
            count = 0;
        }
    }
    
    for (std::set<std::string>::iterator it = plugins.begin(); it != plugins.end(); it++)
    {
        stop_plugin(*it);
    }

    database.save();
    
    return EXIT_SUCCESS;
}
