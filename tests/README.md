# YUI tests

## Layout

```text
tests/
├── unit/            # C unit tests (cmocka)
├── integration/     # JSON + JS (YTest), autoTest: true
│   └── lib/ytest.js
├── e2e/             # reserved
├── visual/          # reserved
└── perf/            # reserved
```

`app/tests/` remains manual playground demos (not in default CI suite).

## Unit (cmocka)

Add `tests/unit/test_foo.c`:

```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static void test_example(void **state) {
    (void)state;
    assert_int_equal(1 + 1, 2);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_example),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

Run one: `ya -r test_foo`

## Integration (YTest)

1. `tests/integration/test-foo.json` with `"autoTest": true`
2. `"js": ["lib/ytest.js", "test-foo.js"]`
3. `onLoad` → `YTest.describe` / `it` → `YTest.run()` → `YTest.exit()`

```js
function onTestLoad() {
  YTest.describe("feature", function () {
    YTest.it("works", function () {
      YTest.expect(1).toBe(1);
    });
  });
  YTest.run();
  YTest.exit();
}
```

## Runner

```bash
python scripts/run_tests.py
python scripts/run_tests.py --unit
python scripts/run_tests.py --integration
python scripts/run_tests.py --filter layer
```

Or: `make test`
