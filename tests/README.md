# YUI tests

## Layout

```text
tests/
├── unit/            # C unit tests (cmocka)
├── integration/     # JSON + JS (YTest), autoTest: true
│   └── lib/ytest.js
├── e2e/             # end-to-end (YUI.click + UI asserts)
├── visual/
│   ├── cases/       # scene JSON + capture JS
│   ├── baselines/   # committed PNG baselines
│   └── output/      # generated screenshots (gitignored)
└── perf/            # frame-budget gates (YUI.perf)
```

`app/tests/` remains manual playground demos (not in default CI suite).

## Unit (cmocka)

Add `tests/unit/test_foo.c` and run `ya -r test_foo`.

## Integration (YTest)

1. `tests/integration/test-foo.json` with `"autoTest": true`
2. `"js": ["lib/ytest.js", "test-foo.js"]`
3. `onLoad` → `YTest.describe` / `it` → `YTest.run()` → `YTest.exit()`

## Headless

Auto tests hide the window by default (`--auto` ⇒ headless).

```bash
# runner (sets YUI_HEADLESS=1)
python scripts/run_tests.py --e2e

# manual
ya -r playground -- --auto tests/e2e/test-button-click.json
ya -r playground -- --auto --show tests/e2e/test-button-click.json   # debug with window
```

Env: `YUI_HEADLESS=1` / `0`, flags: `--headless` / `--show`.

## E2E

`tests/e2e/*.json` with `autoTest: true`. Drive the real pointer path via `YUI.click(id)` (layer center DOWN+UP), then assert with `YUI.getText` / `YUI.dump`.

```bash
python scripts/run_tests.py --e2e
make test-e2e
```

## Perf

`tests/perf/*.json` with `autoTest: true`. Enable `YUI.perf`, wait a few frames, assert soft budgets (`frameMs` / `renderMs`).

Not in the default suite — run explicitly:

```bash
python scripts/run_tests.py --perf
make test-perf
```

## Visual

1. Add `tests/visual/cases/test-foo.json` (`autoTest: true`) that calls `YUI.screenshot("tests/visual/output/foo.png")` after a short delay.
2. Prefer solid-color Views (no text) for stable diffs.
3. Capture / update baselines:

```bash
python scripts/run_tests.py --visual --update-baselines
python scripts/run_tests.py --visual
make test-visual
```

Diff tolerates ≤2% pixels with channel delta ≤8.

## Runner

```bash
python scripts/run_tests.py                 # unit + integration
python scripts/run_tests.py --unit
python scripts/run_tests.py --integration
python scripts/run_tests.py --perf
python scripts/run_tests.py --e2e
python scripts/run_tests.py --visual
python scripts/run_tests.py --all
python scripts/run_tests.py --filter layer
```

Or: `make test` / `make test-all` / `make test-e2e`
