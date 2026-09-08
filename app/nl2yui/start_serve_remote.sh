#!/usr/bin/env bash
set -euo pipefail
cd /home/chenty/develop/nl2yui
.venv/bin/pip install -q flask flask-cors -i https://pypi.tuna.tsinghua.edu.cn/simple
pkill -f "python serve.py" 2>/dev/null || true
mkdir -p logs
nohup .venv/bin/python serve.py --device cuda --port 8765 > logs/serve.log 2>&1 &
echo "PID=$!"
sleep 25
tail -20 logs/serve.log
echo "==== health ===="
curl -s http://127.0.0.1:8765/health || true
echo
echo "==== update ===="
curl -s http://127.0.0.1:8765/api/nl2yui \
  -H "Content-Type: application/json" \
  -d '{"mode":"update","context":"loginBtn:Button:登录","message":"登录按钮改成登录中并禁用"}' || true
echo
echo "==== full ===="
curl -s http://127.0.0.1:8765/api/nl2yui \
  -H "Content-Type: application/json" \
  -d '{"mode":"full","message":"做一个登录页"}' | head -c 500 || true
echo
