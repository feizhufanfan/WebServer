//
// Created by du-pc on 2024/1/19.
//

#ifndef AGENT_NACOSCLIENT_H
#define AGENT_NACOSCLIENT_H

#ifdef __GNUC__
#include <Nacos.h>
#endif
#include <Logger.h>
#include <memory>
#include <Util/mini.h>
#include <Util/util.h>
#include <Common/config.h>

extern feifei::Logger logger;

class NacosClient:  public std::enable_shared_from_this<NacosClient> {
public:
    explicit NacosClient(const char* ServerIP,int ServerPort,int ClientPort,int isLocalStart);
    ~NacosClient();


public:
    void registerInstance();



private:
    std::string mServerIP;
    int mServerPort;
    int mClientPort;
    nacos::Properties configProps;
    nacos::Instance instance;

    nacos::INacosServiceFactory *factory ;
    nacos::ResourceGuard<nacos::INacosServiceFactory> *_guardFactory;
    nacos::NamingService *mNamingSvc;
    nacos::ResourceGuard <nacos::NamingService> *_Nameserviceguard;
    nacos::ConfigService *getconf;
    nacos::ResourceGuard <nacos::ConfigService> *_Configserviceguard;
};


#endif //AGENT_NACOSCLIENT_H
