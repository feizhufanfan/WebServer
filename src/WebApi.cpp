/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <sys/stat.h>
#include <math.h>
#include <signal.h>
#include <functional>
#include <unordered_map>
#include "Util/util.h"
#include <Logger.h>
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"

#include "Common/config.h"

#include "HttpRequester.h"
#include "HttpSession.h"
#include "Network/TcpServer.h"
#include "Network/UdpServer.h"

#include "Util/MD5.h"
#include "WebApi.h"
#include "Thread/WorkThreadPool.h"

#ifdef _WIN32
#include <io.h>
#include <iostream>
#include <tchar.h>
#endif // _WIN32

#if defined(ENABLE_VERSION)
#include "version.h"
#endif
extern feifei::Logger logger;

using namespace std;
using namespace Json;
using namespace toolkit;
using namespace mediakit;

namespace API {
#define API_FIELD "api."
const string kApiDebug = API_FIELD"apiDebug";
const string kSecret = API_FIELD"secret";
const string kSnapRoot = API_FIELD"snapRoot";
const string kDefaultSnap = API_FIELD"defaultSnap";

static onceToken token([]() {
    mINI::Instance()[kApiDebug] = "1";
    mINI::Instance()[kSecret] = "035c73f7-bb6b-4889-a715-d9eb2d1925cc";
    mINI::Instance()[kSnapRoot] = "./www/snap/";
    mINI::Instance()[kDefaultSnap] = "./www/logo.png";
});
}//namespace API

using HttpApi = function<void(const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender)>;
//http api列表
static map<string, HttpApi> s_map_api;

static void responseApi(const Json::Value &res, const HttpSession::HttpResponseInvoker &invoker){
    GET_CONFIG(string, charSet, Http::kCharSet);
    HttpSession::KeyValue headerOut;
    headerOut["Content-Type"] = string("application/json; charset=") + charSet;
    invoker(200, headerOut, res.toStyledString());
};

static void responseApi(int code, const string &msg, const HttpSession::HttpResponseInvoker &invoker){
    Json::Value res;
    res["code"] = code;
    res["msg"] = msg;
    responseApi(res, invoker);
}

static ApiArgsType getAllArgs(const Parser &parser);

static HttpApi toApi(const function<void(API_ARGS_MAP_ASYNC)> &cb) {
    return [cb](const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender) {
        GET_CONFIG(string, charSet, Http::kCharSet);
        HttpSession::KeyValue headerOut;
        headerOut["Content-Type"] = string("application/json; charset=") + charSet;

        Json::Value val;
        val["code"] = API::Success;

        //参数解析成map
        auto args = getAllArgs(parser);
        cb(sender, headerOut, HttpAllArgs<decltype(args)>(parser, args), val, invoker);
    };
}

static HttpApi toApi(const function<void(API_ARGS_MAP)> &cb) {
    return toApi([cb](API_ARGS_MAP_ASYNC) {
        cb(API_ARGS_VALUE);
        invoker(200, headerOut, val.toStyledString());
    });
}

static HttpApi toApi(const function<void(API_ARGS_JSON_ASYNC)> &cb) {
    return [cb](const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender) {
        GET_CONFIG(string, charSet, Http::kCharSet);
        HttpSession::KeyValue headerOut;
        headerOut["Content-Type"] = string("application/json; charset=") + charSet;

        Json::Value val;
        val["code"] = API::Success;

        if (parser["Content-Type"].find("application/json") == string::npos) {
            throw InvalidArgsException("该接口只支持json格式的请求");
        }
        //参数解析成json对象然后处理
        Json::Value args;
        Json::Reader reader;
        reader.parse(parser.Content(), args);

        cb(sender, headerOut, HttpAllArgs<decltype(args)>(parser, args), val, invoker);
    };
}

static HttpApi toApi(const function<void(API_ARGS_JSON)> &cb) {
    return toApi([cb](API_ARGS_JSON_ASYNC) {
        cb(API_ARGS_VALUE);
        invoker(200, headerOut, val.toStyledString());
    });
}

static HttpApi toApi(const function<void(API_ARGS_STRING_ASYNC)> &cb) {
    return [cb](const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, SockInfo &sender) {
        GET_CONFIG(string, charSet, Http::kCharSet);
        HttpSession::KeyValue headerOut;
        headerOut["Content-Type"] = string("application/json; charset=") + charSet;

        Json::Value val;
        val["code"] = API::Success;

        cb(sender, headerOut, HttpAllArgs<string>(parser, (string &)parser.Content()), val, invoker);
    };
}

static HttpApi toApi(const function<void(API_ARGS_STRING)> &cb) {
    return toApi([cb](API_ARGS_STRING_ASYNC) {
        cb(API_ARGS_VALUE);
        invoker(200, headerOut, val.toStyledString());
    });
}

void api_regist(const string &api_path, const function<void(API_ARGS_MAP)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_MAP_ASYNC)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_JSON)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_JSON_ASYNC)> &func) {
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_STRING)> &func){
    s_map_api.emplace(api_path, toApi(func));
}

void api_regist(const string &api_path, const function<void(API_ARGS_STRING_ASYNC)> &func){
    s_map_api.emplace(api_path, toApi(func));
}

//获取HTTP请求中url参数、content参数
static ApiArgsType getAllArgs(const Parser &parser) {
    ApiArgsType allArgs;
    if (parser["Content-Type"].find("application/x-www-form-urlencoded") == 0) {
        auto contentArgs = parser.parseArgs(parser.Content());
        for (auto &pr : contentArgs) {
            allArgs[pr.first] = HttpSession::urlDecode(pr.second);
        }
    } else if (parser["Content-Type"].find("application/json") == 0) {
        try {
            stringstream ss(parser.Content());
            Value jsonArgs;
            ss >> jsonArgs;
            auto keys = jsonArgs.getMemberNames();
            for (auto key = keys.begin(); key != keys.end(); ++key) {
                allArgs[*key] = jsonArgs[*key].asString();
            }
        } catch (std::exception &ex) {
            LOG_WARN(logger,"%s" , ex.what());
        }
    } else if (!parser["Content-Type"].empty()) {
        LOG_WARN(logger,"invalid Content-Type: %s",parser["Content-Type"].c_str());
    }

    for (auto &pr :  parser.getUrlArgs()) {
        allArgs[pr.first] = pr.second;
    }
    return allArgs;
}

extern uint64_t getTotalMemUsage();
extern uint64_t getTotalMemBlock();
extern uint64_t getThisThreadMemUsage();
extern uint64_t getThisThreadMemBlock();
extern std::vector<size_t> getBlockTypeSize();
extern uint64_t getTotalMemBlockByType(int type);
extern uint64_t getThisThreadMemBlockByType(int type) ;

static void *web_api_tag = nullptr;

static inline void addHttpListener(){
    GET_CONFIG(bool, api_debug, API::kApiDebug);
    //注册监听kBroadcastHttpRequest事件
    NoticeCenter::Instance().addListener(&web_api_tag, Broadcast::kBroadcastHttpRequest, [](BroadcastHttpRequestArgs) {
        auto it = s_map_api.find(parser.Url());
        if (it == s_map_api.end()) {
            return;
        }
        //该api已被消费
        consumed = true;

        if(api_debug){
            auto newInvoker = [invoker, parser](int code, const HttpSession::KeyValue &headerOut, const HttpBody::Ptr &body) {
                //body默认为空
                ssize_t size = 0;
                if (body && body->remainSize()) {
                    //有body，获取body大小
                    size = body->remainSize();
                }


                LOG_DBG(logger,"http api debug\r\n# request:\r\n %s   %s\r\n# header:\r\n",parser.Method().c_str(),parser.FullUrl().c_str());
                for (auto &pr : parser.getHeader()) {
                    LOG_DBG(logger,"%s:%s\r\n",pr.first.c_str(),pr.second.c_str());
                }

                auto &content = parser.Content();

                LOG_DBG(logger,"#content: %s ",(content.size() > 4 * 1024 ? content.substr(0, 4 * 1024) : content).c_str());
                if (size > 0 && size < 4 * 1024) {
                    auto response = body->readData(size);
                    LOG_DBG(logger,"# response:\r\n %s",response->data());
                    invoker(code, headerOut, response);
                } else {
                    LOG_DBG(logger,"# response size:\r\n %d",size);
                    invoker(code, headerOut, body);
                }
            };
            ((HttpSession::HttpResponseInvoker &) invoker) = newInvoker;
        }

        try {
            it->second(parser, invoker, sender);
        } catch (ApiRetException &ex) {
            responseApi(ex.code(), ex.what(), invoker);
        }
        catch (std::exception &ex) {
            responseApi(API::Exception, ex.what(), invoker);
        }
    });
}

static void fillSockInfo(Value& val, SockInfo* info) {
    val["peer_ip"] = info->get_peer_ip();
    val["peer_port"] = info->get_peer_port();
    val["local_port"] = info->get_local_port();
    val["local_ip"] = info->get_local_ip();
    val["identifier"] = info->getIdentifier();
}

void getStatisticJson(const function<void(Value &val)> &cb) {
    auto obj = std::make_shared<Value>(objectValue);
    auto &val = *obj;

    val["TcpServer"] = (Json::UInt64)(ObjectStatistic<TcpServer>::count());
    val["TcpSession"] = (Json::UInt64)(ObjectStatistic<TcpSession>::count());
    val["UdpServer"] = (Json::UInt64)(ObjectStatistic<UdpServer>::count());
    val["UdpSession"] = (Json::UInt64)(ObjectStatistic<UdpSession>::count());
    val["TcpClient"] = (Json::UInt64)(ObjectStatistic<TcpClient>::count());
    val["Socket"] = (Json::UInt64)(ObjectStatistic<Socket>::count());



    val["Buffer"] = (Json::UInt64)(ObjectStatistic<Buffer>::count());
    val["BufferRaw"] = (Json::UInt64)(ObjectStatistic<BufferRaw>::count());
    val["BufferLikeString"] = (Json::UInt64)(ObjectStatistic<BufferLikeString>::count());
    val["BufferList"] = (Json::UInt64)(ObjectStatistic<BufferList>::count());



#ifdef ENABLE_MEM_DEBUG
    auto bytes = getTotalMemUsage();
    val["totalMemUsage"] = (Json::UInt64) bytes;
    val["totalMemUsageMB"] = (int) (bytes / 1024 / 1024);
    val["totalMemBlock"] = (Json::UInt64) getTotalMemBlock();
    static auto block_type_size = getBlockTypeSize();
    {
        int i = 0;
        string str;
        size_t last = 0;
        for (auto sz : block_type_size) {
            str.append(to_string(last) + "~" + to_string(sz) + ":" + to_string(getTotalMemBlockByType(i++)) + ";");
            last = sz;
        }
        str.pop_back();
        val["totalMemBlockTypeCount"] = str;
    }

    auto thread_size = EventPollerPool::Instance().getExecutorSize() + WorkThreadPool::Instance().getExecutorSize();
    std::shared_ptr<vector<Value> > thread_mem_info = std::make_shared<vector<Value> >(thread_size);

    shared_ptr<void> finished(nullptr, [thread_mem_info, cb, obj](void *) {
        for (auto &val : *thread_mem_info) {
            (*obj)["threadMem"].append(val);
        }
        //触发回调
        cb(*obj);
    });

    auto pos = 0;
    auto lam0 = [&](TaskExecutor &executor) {
        auto &val = (*thread_mem_info)[pos++];
        executor.async([finished, &val]() {
            auto bytes = getThisThreadMemUsage();
            val["threadName"] = getThreadName();
            val["threadMemUsage"] = (Json::UInt64) bytes;
            val["threadMemUsageMB"] = (Json::UInt64) (bytes / 1024 / 1024);
            val["threadMemBlock"] = (Json::UInt64) getThisThreadMemBlock();
            {
                int i = 0;
                string str;
                size_t last = 0;
                for (auto sz : block_type_size) {
                    str.append(to_string(last) + "~" + to_string(sz) + ":" + to_string(getThisThreadMemBlockByType(i++)) + ";");
                    last = sz;
                }
                str.pop_back();
                val["threadMemBlockTypeCount"] = str;
            }
        });
    };
    auto lam1 = [lam0](const TaskExecutor::Ptr &executor) {
        lam0(*executor);
    };
    EventPollerPool::Instance().for_each(lam1);
    WorkThreadPool::Instance().for_each(lam1);
#else
    cb(*obj);
#endif
}



template <typename Type>
static void getArgsValue(const HttpAllArgs<ApiArgsType> &allArgs, const string &key, Type &value) {
    auto val = allArgs[key];
    if (!val.empty()) {
        value = (Type)val;
    }
}

/**
 * 安装api接口
 * 所有api都支持GET和POST两种方式
 * POST方式参数支持application/json和application/x-www-form-urlencoded方式
 */
void installWebApi() {
    addHttpListener();
    GET_CONFIG(string,api_secret,API::kSecret);




    static auto s_get_api_list = [](API_ARGS_MAP){
        CHECK_SECRET();
        for(auto &pr : s_map_api){
            val["data"].append(pr.first);
        }
    };


#if !defined(_WIN32)
    //重启服务器,只有Daemon方式才能重启，否则是直接关闭！
    //测试url http://127.0.0.1/index/api/restartServer
    api_regist("/index/api/restartServer",[](API_ARGS_MAP){
        CHECK_SECRET();
        EventPollerPool::Instance().getPoller()->doDelayTask(1000,[](){
            //尝试正常退出
            ::kill(getpid(), SIGINT);

            //3秒后强制退出
            EventPollerPool::Instance().getPoller()->doDelayTask(3000,[](){
                exit(0);
                return 0;
            });

            return 0;
        });
        val["msg"] = "服务器将在一秒后自动重启";
    });
#else
    //增加Windows下的重启代码
    api_regist("/index/api/restartServer", [](API_ARGS_MAP) {
        CHECK_SECRET();
        //创建重启批处理脚本文件
        FILE *pf;
        errno_t err = ::_wfopen_s(&pf, L"RestartServer.cmd", L"w"); //“w”如果该文件存在，其内容将被覆盖
        if (err == 0) {
            char szExeName[1024];
            char drive[_MAX_DRIVE] = { 0 };
            char dir[_MAX_DIR] = { 0 };
            char fname[_MAX_FNAME] = { 0 };
            char ext[_MAX_EXT] = { 0 };
            char exeName[_MAX_FNAME] = { 0 };
            GetModuleFileNameA(NULL, szExeName, 1024); //获取进程的全路径
            _splitpath(szExeName, drive, dir, fname, ext);
            strcpy(exeName, fname);
            strcat(exeName, ext);
            fprintf(pf, "@echo off\ntaskkill /f /im %s\nstart \"\" \"%s\"\ndel %%0", exeName, szExeName);
            fclose(pf);
            // 1秒后执行创建的批处理脚本
            EventPollerPool::Instance().getPoller()->doDelayTask(1000, []() {
                STARTUPINFO si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof si);
                ZeroMemory(&pi, sizeof pi);
                si.cb = sizeof si;
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                TCHAR winSysDir[1024];
                ZeroMemory(winSysDir, sizeof winSysDir);
                GetSystemDirectory(winSysDir, 1024);
                TCHAR appName[1024];
                ZeroMemory(appName, sizeof appName);

                _stprintf(appName, "%s\\cmd.exe", winSysDir);
                BOOL bRet = CreateProcess(appName, " /c RestartServer.cmd", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

                if (bRet == FALSE) {
                    int err = GetLastError();
                    cout << endl << "无法执行重启操作，错误代码：" << err << endl;
                }
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return 0;
            });
            val["msg"] = "服务器将在一秒后自动重启";
        } else {
            val["msg"] = "创建重启脚本文件失败";
            val["code"] = API::OtherFailed;
        }
    });
#endif//#if !defined(_WIN32)

    //获取所有TcpSession列表信息
    //可以根据本地端口和远端ip来筛选
    //测试url(筛选某端口下的tcp会话) http://127.0.0.1/index/api/getAllSession?local_port=1935
    api_regist("/index/api/getAllSession",[](API_ARGS_MAP){
        CHECK_SECRET();
        Value jsession;
        uint16_t local_port = allArgs["local_port"].as<uint16_t>();
        string peer_ip = allArgs["peer_ip"];

        SessionMap::Instance().for_each_session([&](const string &id,const Session::Ptr &session){
            if(local_port != 0 && local_port != session->get_local_port()){
                return;
            }
            if(!peer_ip.empty() && peer_ip != session->get_peer_ip()){
                return;
            }
            fillSockInfo(jsession, session.get());
            jsession["id"] = id;
            jsession["typeid"] = toolkit::demangle(typeid(*session).name());
            val["data"].append(jsession);
        });
    });
    //断开tcp连接，比如说可以断开rtsp、rtmp播放器等
    //测试url http://127.0.0.1/index/api/kick_session?id=123456
    api_regist("/index/api/kick_session",[](API_ARGS_MAP){
        CHECK_SECRET();
        CHECK_ARGS("id");
        //踢掉tcp会话
        auto session = SessionMap::Instance().get(allArgs["id"]);
        if(!session){
            throw ApiRetException("can not find the target",API::OtherFailed);
        }
        session->safeShutdown();
    });
    //批量断开tcp连接，比如说可以断开rtsp、rtmp播放器等
    //测试url http://127.0.0.1/index/api/kick_sessions?local_port=1935
    api_regist("/index/api/kick_sessions",[](API_ARGS_MAP){
        CHECK_SECRET();
        uint16_t local_port = allArgs["local_port"].as<uint16_t>();
        string peer_ip = allArgs["peer_ip"];
        size_t count_hit = 0;

        list<Session::Ptr> session_list;
        SessionMap::Instance().for_each_session([&](const string &id,const Session::Ptr &session){
            if(local_port != 0 && local_port != session->get_local_port()){
                return;
            }
            if(!peer_ip.empty() && peer_ip != session->get_peer_ip()){
                return;
            }
            session_list.emplace_back(session);
            ++count_hit;
        });

        for(auto &session : session_list){
            session->safeShutdown();
        }
        val["count_hit"] = (Json::UInt64)count_hit;
    });
    api_regist("/index/api/getStatistic",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        getStatisticJson([headerOut, val, invoker](const Value &data) mutable{
            val["data"] = data;
            invoker(200, headerOut, val.toStyledString());
        });
    });
#if defined(ENABLE_VERSION)
    api_regist("/index/api/version",[](API_ARGS_MAP_ASYNC){
        CHECK_SECRET();
        Value ver;
        ver["buildTime"] = BUILD_TIME;
        ver["branchName"] = BRANCH_NAME;
        ver["commitHash"] = COMMIT_HASH;
        val["data"] = ver;
        invoker(200, headerOut, val.toStyledString());
    });
#endif
    static auto checkAccess = [](const string &params){
        //我们假定大家都要权限访问
        return true;
    };

    api_regist("/index/hook/on_http_access",[](API_ARGS_MAP){
        //在这里根据allArgs["params"](url参数)来判断该http客户端是否有权限访问该文件
        if(!checkAccess(allArgs["params"])){
            //无访问权限
            val["err"] = "无访问权限";
            //仅限制访问当前目录
            val["path"] = "";
            //标记该客户端无权限1分钟
            val["second"] = 60;
            return;
        }

        //可以访问
        val["err"] = "";
        //只能访问当前目录
        val["path"] = "";
        //该http客户端用户被授予10分钟的访问权限，该权限仅限访问当前目录
        val["second"] = 10 * 60;
    });
}

void unInstallWebApi(){
    NoticeCenter::Instance().delListener(&web_api_tag);
}
