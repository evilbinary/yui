// 测试 C 函数调用

// 1. 测试标准 print 函数
print("Test 1: Standard print function");
print("Hello from JavaScript!");

// 2. 测试 Math 函数
print("Test 2: Math functions");
print("Math.random() =", Math.random());
print("Math.floor(3.14) =", Math.floor(3.14));
print("Math.floor(Math.random() * 10) =", Math.floor(Math.random() * 10));

// 3. 测试 Array 操作
print("Test 3: Array operations");
var arr = [1, 2, 3, 4, 5];
print("Array:", arr);
print("Array length:", arr.length);
arr.push(6);
print("Array after push:", arr);

// 4. 测试对象操作
print("Test 4: Object operations");
var obj = { name: "test", value: 42 };
print("Object:", obj.name, obj.value);

// 5. 测试函数定义和调用
print("Test 5: Function definition and call");
function add(a, b) {
    return a + b;
}
print("add(3, 5) =", add(3, 5));

// 6. 测试闭包
print("Test 6: Closure");
function makeCounter() {
    var count = 0;
    return function() {
        count++;
        return count;
    };
}
var counter = makeCounter();
print("counter() =", counter());
print("counter() =", counter());
print("counter() =", counter());

print("All tests completed successfully!");
