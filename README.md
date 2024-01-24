# 基于ZLMediaKit剪裁的简易的HTTP服务器

## 实现功能
在传统的http服务器的基础上增加nacos服务对http服务器的监测，实现实时修改http服务器的配置并完成热重启。
其中对ZLMediaKit部分做了部分删除和修改，使其功能更具有专一性。
目前Nacos暂时支持Linux
## 编译方式

```cmake
cmake -S . -B build 
cd build
make 
```


## 运行
`./bin/WebServer ./config.ini`

# 参考项目
[ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit)  

[ZLToolKit](https://github.com/ZLMediaKit/ZLToolKit)



