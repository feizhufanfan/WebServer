//
// Created by du-pc on 2024/1/19.
//

#include "NacosClient.h"
#include "Util/onceToken.h"
#include "Util/util.h"



NacosClient::NacosClient(const char* ServerIP,int ServerPort,int ClientPort,int isLocalStart=0):mServerIP(ServerIP),mServerPort(ServerPort),mClientPort(ClientPort) {

    try {
        configProps[nacos::PropertyKeyConst::SERVER_ADDR] = mServerIP + ":" + std::to_string(mServerPort);
        configProps[nacos::PropertyKeyConst::UDP_RECEIVER_PORT] = std::to_string(mClientPort);
        factory = nacos::NacosFactoryFactory::getNacosFactory(configProps);
        _guardFactory=new nacos::ResourceGuard<nacos::INacosServiceFactory>(factory);
        if(factory == nullptr) {
            LOG_ERR(logger,"Create NacosServiceFactory Failed, NacosSvrAddr:%s",configProps[nacos::PropertyKeyConst::SERVER_ADDR].c_str());
            return;
        }
        LOG_INFO(logger,"begin CreateNamingService...");
        do{
            mNamingSvc =factory->CreateNamingService();
            if(mNamingSvc == NULL) {
                std::cout << "CreateNamingService Failed" << std::endl;
                break;
            }
            _Nameserviceguard=new nacos::ResourceGuard<nacos::NamingService>(mNamingSvc);
            LOG_INFO(logger,"begin CreateConfigService...");
            getconf = factory->CreateConfigService();
            if(getconf == NULL) {
                LOG_ERR(logger,"CreateConfigService Failed");
                break;
            }
            _Configserviceguard=new nacos::ResourceGuard<nacos::ConfigService>(getconf);
            if(!isLocalStart){
                LOG_INFO(logger,"begin Initialize nacos configfile...");
                auto nacosconfig = getconf->getConfig(toolkit::mINI::Instance()[Nacos::kDataId], toolkit::mINI::Instance()[Nacos::kGroupName], 1500);
                if (nacosconfig == "") {
                    LOG_ERR(logger,"GetConfig Failed From Nacos, NacosDataId: %s, NacosGroup: %s",toolkit::mINI::Instance()[Nacos::kDataId].c_str(), toolkit::mINI::Instance()[Nacos::kGroupName].c_str());
                    break;
                }
                //nacos配置回显到本地

            }



        } while (false);
    }catch (nacos::NacosException &e){
        LOG_ERR(logger,"\t\tstd::cout << \"Init_Nacos Throw NacosException, Errcode:%d , Reason:%s",e.errorcode(),e.what());
    }catch (...){
        LOG_ERR(logger,"Init_Nacos Throw Exception");
    }



}

void NacosClient::registerInstance() {
    instance.ip = mServerIP;
    instance.port = mServerPort;
    std::stringstream ss;
    ss<< mServerIP << "#" <<mServerPort <<"#DEFAULT#DEFAULT@@WebServer";
    instance.instanceId = ss.str();
    instance.metadata["version"] = "V3.1.0";
    mNamingSvc->registerInstance("WebServer", "DEFAULT", instance);
}

NacosClient::~NacosClient() {
    try {
        mNamingSvc->deregisterInstance("AGENT", "DEFAULT", instance);
        getconf->removeConfig("AGENT", "DEFAULT");
#if _win32
        toolkit::sleep(1);
#else
        sleep(1);
#endif
    }catch (nacos::NacosException &e){
        LOG_ERR(logger, "encounter exception while registering service instance, raison: %s" , e.what());
    }
    if (_Configserviceguard!= nullptr)delete _Configserviceguard;
    if (_Nameserviceguard!= nullptr)delete _Nameserviceguard;
    if (_guardFactory!= nullptr)delete _guardFactory;

}
