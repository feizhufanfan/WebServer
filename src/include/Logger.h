//
// Created by du-pc on 2024/1/19.
//
#ifndef LOGGER_H
#define LOGGER_H


#include <iostream>
#include <sstream>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <stdarg.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <type_traits>
#if _WIN32
#include <direct.h>
#include <ctime>
#pragma warning(disable: 4828)
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <atomic>
#include <cstring>
#include <sys/time.h>
#endif

#ifdef _WIN32
#define FILENAME (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif


#define RECIVE_BUFFERSIZE 10240
//是否启用单例模式
#define ISSINGNEL 0


//颜色宏定义
#define NONE         "\033[m"
#define RED          "\033[0;32;31m"
#define LIGHT_RED    "\033[1;31m"
#define GREEN        "\033[0;32;32m"
#define LIGHT_GREEN  "\033[1;32m"
#define BLUE         "\033[0;32;34m"
#define LIGHT_BLUE   "\033[1;34m"
#define DARY_GRAY    "\033[1;30m"
#define CYAN         "\033[0;36m"
#define LIGHT_CYAN   "\033[1;36m"
#define PURPLE       "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN        "\033[0;33m"
#define YELLOW       "\033[1;33m"
#define LIGHT_GRAY   "\033[0;37m"
#define WHITE        "\033[1;37m"
#if _WIN32
#define LOG_DBG(log,str,...)  do{log.Print(PURPLE,"[log D] \033[m",FILENAME,__LINE__,str,__VA_ARGS__); }while(0)
#define LOG_DEGCout(log);          log<<PURPLE<<"[log D] \033[m"<<FILENAME<<__LINE__
#define LOG_INFO(log,str,...) do{log.Print(GREEN,"[log I] \033[m",FILENAME,__LINE__,str,__VA_ARGS__); }while(0)
#define LOG_INFOCount(log)         log<<GREEN<<"[log I] \033[m"<<FILENAME<<__LINE__
#define LOG_WARN(log,str,...) do{log.Print(YELLOW,"[log W] \033[m",FILENAME,__LINE__,str,__VA_ARGS__); }while(0)
#define LOG_WARNCout(log)     log<<YELLOW<<"[log W] \033[m"<<FILENAME<<__LINE__
#define LOG_ERR(log,str,...)  do{log.Print(LIGHT_RED,"[log E] \033[m",FILENAME,__LINE__,str,__VA_ARGS__); }while(0)
#define LOG_ERRCout(log)          log<<LIGHT_RED<<"[log E] \033[m"<<FILENAME<<__LINE__
#define LOG_TraceL(log) log<<BLUE<<"[log E] \033[m"<<FILENAME<<__LINE__
#else
#define LOG_DBG(log,str,...)  do{log.Print(PURPLE,"[log D] \033[m",FILENAME,__LINE__,str,##__VA_ARGS__); }while(0)
#define LOG_DEGCout(log);          log<<PURPLE<<"[log D] \033[m"<<FILENAME<<__LINE__
#define LOG_INFO(log,str,...) do{log.Print(GREEN,"[log I] \033[m",FILENAME,__LINE__,str,##__VA_ARGS__); }while(0)
#define LOG_INFOCount(log)         log<<GREEN<<"[log I] \033[m"<<FILENAME<<__LINE__
#define LOG_WARN(log,str,...) do{log.Print(YELLOW,"[log W] \033[m",FILENAME,__LINE__,str,##__VA_ARGS__); }while(0)
#define LOG_WARNCout(log)     log<<YELLOW<<"[log W] \033[m"<<FILENAME<<__LINE__

#define LOG_ERR(log,str,...)  do{log.Print(LIGHT_RED,"[log E] \033[m",FILENAME,__LINE__,str,##__VA_ARGS__); }while(0)
#define LOG_ERRCout(log)          log<<LIGHT_RED<<"[log E] \033[m"<<FILENAME<<__LINE__
#define LOG_TraceL(log)       log<<BLUE<<"[log E] \033[m"<<FILENAME<<__LINE__
#endif


namespace feifei {
    class Logger {
    public:
#if ISSINGNEL

        static Logger &getinstance(std::string Path) {
            static Logger *instance = nullptr;
            if (!instance) {  // 第一次检查
                std::lock_guard<std::mutex> lock(mutex_);
                if (!instance) {  // 第二次检查
                    instance = new Logger(Path);
                }
            }
            return *instance;
        }

        inline void setPath(std::string Path = "./") {
            this->m_filePath = Path;
        }

    private:
        static std::mutex mutex_;//单例模式
        Logger() = delete;

    public:
        Logger(const Logger &) = delete;

        Logger &operator=(const Logger &) = delete;

#endif

        template<typename T>
        Logger& operator<<(const T& t) {
            std::unique_lock<std::mutex> lock(m_mutex);
            std::ostringstream oss;
            oss << t;
            m_queue.emplace(oss.str());
            m_condition.notify_all();
            return *this;
        }



        template<typename... Args>
        void PrintLog(const char *format, Args... args) {
            std::unique_lock<std::mutex> lock(m_mutex);
            std::ostringstream stream;
            std::shared_ptr<char> buffer(new char[RECIVE_BUFFERSIZE]);
            memcpy(buffer.get(), "", RECIVE_BUFFERSIZE);

#if _WIN32
            sprintf_s(buffer.get(), RECIVE_BUFFERSIZE, format, args...);
#else
            snprintf(buffer.get(),RECIVE_BUFFERSIZE,format,args...);
#endif


            stream << buffer;
            m_queue.emplace(stream.str());
            m_condition.notify_all();
        }
        template<typename... Args>
        void Print(const char* Color,const char* logType,std::string filename,int line,const char *format, Args... args) {
            std::unique_lock<std::mutex> lock(m_mutex);
            std::ostringstream stream;
            std::shared_ptr<char> buffer=std::shared_ptr<char>(new char[RECIVE_BUFFERSIZE],[](char* ptr){
                delete[] ptr;
            });
           // char* RawBuffer_ptr=buffer.get();
            memcpy(buffer.get(), "", RECIVE_BUFFERSIZE);
           // buffer.reset(RawBuffer_ptr);
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);



#if _WIN32
            std::tm now_tm;
            localtime_s(&now_tm,&time);
            sprintf_s(buffer.get(), RECIVE_BUFFERSIZE, format, args...);
#else
            struct timeval tv;
            struct tm* ptm;
            char time_string[40];
            gettimeofday(&tv, NULL);
            ptm = localtime (&(tv.tv_sec));
            strftime (time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", ptm);  //输出格式为: 2022-03-30 20:38:37

#endif
            snprintf(buffer.get(),RECIVE_BUFFERSIZE,format,args...);
#if _WIN32
            stream <<Color<<std::put_time(&now_tm, "%Y-%m-%d %T")<<" "<<filename<<":"<<line<<" "<<logType<<" "<<buffer;
#else
            stream <<Color<<time_string<<" "<<filename<<":"<<line<<" "<<logType<<" "<<buffer;
#endif

            m_queue.emplace(stream.str());
            m_condition.notify_all();
            buffer.reset();
        }


#if ISSINGNEL
        private:
#else

    public:
#endif

        Logger(std::string filePath) : m_stop(false), m_filePath(filePath) {
            m_thread = std::thread(&Logger::processEntries, this);
            create_directory(filePath);
        }

        ~Logger() {
            m_stop.store(true);
            m_condition.notify_all();
            m_thread.join();
        }

    private:


        bool create_directory(const std::string &path) {
#if _WIN32
            int status = _mkdir(path.c_str());
#else
            int status = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

            if (status == 0) {
                return true;
            } else {
                switch (errno) {
                    case ENOENT:
                        // parent didn't exist, try to create it
#if _WIN32
                        if (create_directory(path.substr(0, path.find_last_of('\\')))) {
#else
                        if (create_directory(path.substr(0, path.find_last_of('/')))) {
#endif
                            // try again to create the directory
#if _WIN32
                            return 0 == _mkdir(path.c_str());
#else
                            return 0 == mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

                        } else {
                            return false;
                        }
                        break;
                    case EEXIST:
                        // done!
                        return true;
                    default:
                        return false;
                }
            }
        }

        void processEntries() {
            while (true) {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this]() { return !m_queue.empty() || m_stop; });
                if (m_stop.load()) {
                    break;
                }

#if _WIN32
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                std::tm now_tm;
                localtime_s(&now_tm,&time);
#else
                struct timeval tv;
                struct tm* ptm;
                char time_string[40];
                gettimeofday(&tv, NULL);
                ptm = localtime (&(tv.tv_sec));
                strftime (time_string, sizeof(time_string), "%Y-%m-%d", ptm);  //输出格式为: 2022-03-30 20:38:37
#endif

                std::stringstream ss;
#if _WIN32
                ss << m_filePath << std::put_time(&now_tm, "%Y-%m-%d") << ".log";
#else
                ss << m_filePath <<time_string << ".log";
#endif
                std::string filename = ss.str();
                if (filename != m_filename) {
                    m_filename = filename;
                    m_file.close();
                    m_file.open(m_filename, std::ios::app);
                }
                while (!m_queue.empty()) {
                    std::string prestr;
                    m_file << m_queue.front()
                           << std::endl;
                    std::cout  << m_queue.front()
                               << std::endl;
                    m_queue.pop();
                }
            }
        }


    private:
        std::string m_filePath;
        std::string m_filename;
        std::ofstream m_file;
        std::thread m_thread;
        std::mutex m_mutex;
        std::condition_variable m_condition;
        std::queue<std::string> m_queue;
        std::atomic<bool> m_stop;


    };


};
#endif //LOGGER_H