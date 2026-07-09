## 依赖
pacman -S mingw-w64-x86_64-libmariadbclient

## 实现方式
1、可以实现mysqlmodule的api,js context register

2、可以通过事件注册，直接在js里面使用c函数