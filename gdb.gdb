# 关闭确认提示
set confirm off
# 禁用分页显示
set pagination off
# 设置日志文件
set logging file gdb_crash.log
set logging on
# 配置信号处理：捕获常见崩溃信号
handle SIGSEGV stop print pass
handle SIGABRT stop print pass  
handle SIGILL stop print pass
handle SIGFPE stop print pass
handle SIGBUS stop print pass
# 运行程序
run app/lvgl/calc.json
# 程序崩溃后自动执行backtrace
bt full
# 可选：打印更多调试信息
info registers
info locals
info args
# 退出GDB
quit