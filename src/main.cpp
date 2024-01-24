//
// Created by du-pc on 2024/1/19.
//
#ifdef __GNUC__
#include <NacosClient.h>
#endif
#include <Logger.h>
#include "Util/File.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Common/config.h"
#include <csignal>
#include "Network/TcpServer.h"
#include "Common/config.h"
#include "WebApi.h"
//////////////HTTP配置///////////
namespace Http {
    using namespace std;
#define HTTP_FIELD "http."
    const string kPort = HTTP_FIELD"port";
    const string kSSLPort = HTTP_FIELD"sslport";
    toolkit::onceToken token1([](){
        toolkit::mINI::Instance()[kPort] = 80;
        toolkit::mINI::Instance()[kSSLPort] = 443;
    },nullptr);
}//namespace Http


namespace Nacos{
    using namespace std;
#define NACOS_FILED "nacos."
    const string kServerip = NACOS_FILED"Serverip";
    const string kServerPort= NACOS_FILED"Serverport";
    const string kCliIp =NACOS_FILED"ClientIp";
    const string kCliPort = NACOS_FILED"ClientPort";
    const string kDataId=NACOS_FILED"DataId";
    const string kGroupName=NACOS_FILED"GroupName";
    const string kLocalLoadCfg=NACOS_FILED"isLocalStart";
    toolkit::onceToken token1([](){
        toolkit::mINI::Instance()[kServerPort] = "127.0.0.1";
        toolkit::mINI::Instance()[kServerPort] = 8848;
        toolkit::mINI::Instance()[kCliIp] = "127.0.0.1";
        toolkit::mINI::Instance()[kCliPort] = "8001";
        toolkit::mINI::Instance()[kDataId] = "WebServerConfig";
        toolkit::mINI::Instance()[kGroupName] = "WebServerGroupName";
        toolkit::mINI::Instance()[kLocalLoadCfg] = true;
    }, nullptr);
}


feifei::Logger logger("./WebServer/");
int main(int argc,char* argv[]){
    LOG_INFO(logger,"日志初始化");
    if (argc<2){
        LOG_ERR(logger,"参数错误，未加载配置文件");
        return -1;
    }
    try {
        toolkit::mINI::Instance().parseFile(argv[1]);
    }catch (std::exception &ex){
        LOG_ERR(logger,"配置文件解析错误 %s",ex.what());
        toolkit::mINI::Instance().dumpFile(argv[1]);
        return -2;
    }
    uint16_t httpPort = toolkit::mINI::Instance()[Http::kPort];
    std::string NacosSerIP=toolkit::mINI::Instance()[Nacos::kServerip];
    uint16_t    NacosPort = toolkit::mINI::Instance()[Nacos::kServerPort];
    uint16_t    NacosCliPort=toolkit::mINI::Instance()[Nacos::kCliPort];
    std::string NacosDataId=toolkit::mINI ::Instance()[Nacos::kDataId];
    std::string NacosGroupName=toolkit::mINI ::Instance()[Nacos::kGroupName];
    bool isLocalStartNacos=toolkit::mINI::Instance()[Nacos::kLocalLoadCfg];

#ifdef __GNUC__
    NacosClient nacosClient(NacosSerIP.c_str(),NacosPort,NacosCliPort,isLocalStartNacos);
    nacosClient.registerInstance();
#endif

    auto httpSrv = std::make_shared<toolkit::TcpServer>();;


    try {
        if (httpPort) { httpSrv->start<mediakit::HttpSession>(httpPort); }

    }catch (std::exception &ex){
        LOG_ERR(logger,"创建http服务失败 %s",ex.what());
        return -1;
    }


    installWebApi();
    //设置退出信号处理函数
    static toolkit::semaphore sem;
    signal(SIGINT, [](int) {
        LOG_INFO(logger,"SIGINT:exit");
        signal(SIGINT, SIG_IGN);// 设置退出信号
        sem.post();
    });// 设置退出信号

    sem.wait();
    unInstallWebApi();
#ifndef __GNUC__
    toolkit::sleep(1);
#else
    sleep(1);
#endif
    //休眠1秒再退出，防止资源释放顺序错误
    logger << "程序退出中,请等待...";
    logger << "程序退出完毕!";

    return 0;
}