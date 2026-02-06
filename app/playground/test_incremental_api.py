#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
测试增量更新API接口是否符合 json-update-spec.md 规范
"""

import requests
import json

BASE_URL = "http://localhost:5000"

def test_incremental_api():
    """测试增量更新接口"""
    
    print("=== 测试增量更新API接口 ===\n")
    
    # 测试用例
    test_cases = [
        {
            "name": "基本文本更新",
            "message": "把状态标签的文本改为'测试成功'",
            "expected_keys": ["target", "change"]
        },
        {
            "name": "批量更新",
            "message": "批量更新：项目1改为红色，项目2改为蓝色",
            "expected_type": "array"
        },
        {
            "name": "删除操作",
            "message": "删除第一个列表项",
            "check_null": True
        },
        {
            "name": "属性更新",
            "message": "把按钮背景色改为绿色",
            "expected_change_props": ["bgColor"]
        }
    ]
    
    for i, test_case in enumerate(test_cases, 1):
        print(f"测试 {i}: {test_case['name']}")
        print(f"输入消息: {test_case['message']}")
        
        try:
            response = requests.post(
                f"{BASE_URL}/api/message/incremental",
                json={
                    "message": test_case['message'],
                    "json": {}
                },
                headers={"Content-Type": "application/json"}
            )
            
            if response.status_code == 200:
                result = response.json()
                print(f"✅ 响应状态: {response.status_code}")
                print(f"响应内容: {json.dumps(result, indent=2, ensure_ascii=False)}")
                
                # 验证格式
                validate_response(result, test_case)
                print("✅ 格式验证通过\n")
            else:
                print(f"❌ HTTP错误: {response.status_code}")
                print(f"错误详情: {response.text}\n")
                
        except Exception as e:
            print(f"❌ 请求异常: {str(e)}\n")

def validate_response(result, test_case):
    """验证响应格式是否符合规范"""
    
    # 检查是否为数组或对象
    if isinstance(result, list):
        print("  ✓ 返回格式: 数组 (批量更新)")
        for i, item in enumerate(result):
            validate_single_update(item, f"  项目{i+1}")
    elif isinstance(result, dict):
        print("  ✓ 返回格式: 对象 (单个更新)")
        validate_single_update(result, "  ")
    else:
        print(f"  ❌ 错误格式: 期望数组或对象，得到 {type(result)}")

def validate_single_update(update, prefix=""):
    """验证单个更新对象的格式"""
    
    # 检查必需字段
    if "target" not in update:
        print(f"{prefix}❌ 缺少 target 字段")
        return False
    
    if "change" not in update:
        print(f"{prefix}❌ 缺少 change 字段")
        return False
    
    print(f"{prefix}✓ 包含 target: {update['target']}")
    print(f"{prefix}✓ 包含 change: {list(update['change'].keys())}")
    
    # 检查 change 字段是否为字典
    if not isinstance(update['change'], dict):
        print(f"{prefix}❌ change 字段必须是字典")
        return False
    
    # 检查是否有 null 值（删除操作）
    has_null = any(v is None for v in update['change'].values())
    if has_null:
        print(f"{prefix}✓ 包含删除操作 (null 值)")
    
    return True

def test_spec_compliance():
    """测试是否符合 json-update-spec.md 规范"""
    
    print("=== JSON 更新规范合规性测试 ===\n")
    
    # 测试规范中的示例
    spec_examples = [
        {
            "name": "规范示例1: 单个更新",
            "input": "更新按钮文本为提交",
            "should_contain": ["target", "change", "text"]
        },
        {
            "name": "规范示例2: 批量更新",
            "input": "批量更新按钮和标签",
            "should_contain": ["target", "change"]
        },
        {
            "name": "规范示例3: 删除属性",
            "input": "删除某个元素的可见性属性",
            "should_contain_null": True
        }
    ]
    
    for example in spec_examples:
        print(f"测试: {example['name']}")
        print(f"输入: {example['input']}")
        
        try:
            response = requests.post(
                f"{BASE_URL}/api/message/incremental",
                json={"message": example['input'], "json": {}}
            )
            
            if response.status_code == 200:
                result = response.json()
                print("✅ 请求成功")
                
                # 检查必需字段
                if isinstance(result, list):
                    for item in result:
                        check_spec_fields(item, example)
                else:
                    check_spec_fields(result, example)
                    
            else:
                print(f"❌ 请求失败: {response.status_code}")
                
        except Exception as e:
            print(f"❌ 异常: {str(e)}")
        
        print()

def check_spec_fields(result, example):
    """检查是否包含规范要求的字段"""
    if "should_contain" in example:
        for field in example["should_contain"]:
            if field == "target" and "target" in result:
                print(f"  ✓ 包含 {field}: {result['target']}")
            elif field == "change" and "change" in result:
                print(f"  ✓ 包含 {field}: {list(result['change'].keys())}")
            elif field in result.get("change", {}):
                print(f"  ✓ change 包含 {field}: {result['change'][field]}")
    
    if example.get("should_contain_null"):
        if any(v is None for v in result.get("change", {}).values()):
            print("  ✓ 包含 null 值 (删除操作)")

if __name__ == "__main__":
    print("YUI 增量更新API测试工具")
    print("=" * 50)
    
    try:
        # 先检查服务器是否运行
        status_resp = requests.get(f"{BASE_URL}/api/status", timeout=5)
        if status_resp.status_code == 200:
            print("✅ 服务器连接成功\n")
            test_incremental_api()
            test_spec_compliance()
        else:
            print("❌ 无法连接到服务器")
            print("请先启动API服务器: python api-server.py")
    except requests.exceptions.ConnectionError:
        print("❌ 无法连接到服务器")
        print("请先启动API服务器: python api-server.py")
    except Exception as e:
        print(f"❌ 测试过程中出现错误: {str(e)}")