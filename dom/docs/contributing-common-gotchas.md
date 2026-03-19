# Common gotchas when contributing to DOM

This document covers frequently recurring issues found during DOM code review.
Addressing these upfront will speed up the review process.

## Spec compliance

DOM code implements web specifications. Reviewers will cross-reference your
patch against the relevant spec, and mismatches are one of the most common
reasons for review feedback.

- **Link to the spec algorithm you are implementing.** Add a comment with the
  URL to the relevant section of the spec (WHATWG, W3C, etc.) above non-trivial
  algorithm implementations. This helps both the reviewer and future readers.
- **Reference spec step numbers.** When implementing a multi-step algorithm, use
  comments like `// Step 4.1:` to make it easy to verify correctness. When
  possible, write the steps exactly as they appear in the spec.
- **Match the WebIDL types.** Use the C++ type that corresponds to the WebIDL
  type (e.g. WebIDL `float` → C++ `float`, WebIDL `double` → C++ `double`).
  If the spec defines a dictionary inheritance chain, match it exactly in WebIDL.
- **Match the spec ordering in WebIDL.** Place attributes and methods in your
  `.webidl` file in the same order they appear in the specification. The same
  applies to C++ declarations that mirror a spec interface.
- **Gate new APIs behind a pref.** New or in-progress interfaces should be
  guarded with a `[Pref=...]` or `[Func=...]` annotation in the `.webidl` file
  so they can be enabled incrementally.
- **File spec bugs when you find issues.** If the spec is wrong or unclear, file
  an issue on the relevant spec repo and note the bug number in a code comment.
  Do not silently diverge from the spec without documenting why.

## Memory management and cycle collection

If your change adds pointers between C++ objects, you need to understand cycle
collection. Read `/xpcom/docs/cc-macros.rst` first — it explains when a class
must participate in cycle collection and how to write `Traverse`/`Unlink`. The
reference-counting helpers are documented in `/xpcom/docs/refptr.rst`.

Common review feedback in this area:

- **If your class holds `RefPtr` or `nsCOMPtr` to cycle-collected objects, it
  must participate in cycle collection.** The CC macros doc above walks through
  the steps.
- **Use `WeakPtr` when your object does not need to keep another alive.** The
  owned object should extend `SupportsWeakPtr`.
- **Do not add `RefPtr` where lifetime is already guaranteed** by the caller or
  parent. A raw pointer or reference may be correct.
- **Clear pointers to short-lived objects explicitly** (e.g. a pointer to a
  `PresShell` must be nulled when the shell goes away).

## Naming conventions

Mozilla has specific C++ naming conventions. Inconsistencies are frequently
flagged in review.

- **Member variables**: Use `mFoo` prefix.
- **Function parameters**: Use `aFoo` prefix.
- **Method names should describe what they do.** If a method checks a condition
  and returns a boolean, the name should reflect that (e.g. `CanRunJS()` instead
  of a generic `Check()`). Prefer clarity over brevity.
- **Follow existing naming patterns.** If similar APIs in the codebase use
  `GetOrCreate*` and `GetExisting*`, follow that convention rather than inventing
  new names.
- **Member variables go after methods** in class declarations.

## Patch scope

Keep patches focused. This makes review faster and reduces risk.

- **Do not include unrelated changes.** If you notice something to fix while
  working on a patch, file a separate bug and address it in a followup.
- **File followup bugs explicitly.** When leaving a TODO or intentionally
  deferring part of the implementation, file a bug and reference the bug number
  in a comment.

## Thread safety

Most DOM code runs on the main thread. If your change introduces shared mutable
state or off-main-thread access, read `/xpcom/docs/thread-safety.rst` for
Gecko's thread-safety annotation system (`MOZ_GUARDED_BY`, etc.).

- **Do not use atomics on main-thread-only code.** Plain booleans and integers
  are sufficient when the class is only used on the main thread.
- **Document your threading model.** If shared mutable state is intentional, add
  a comment explaining what guarantees safety (e.g. which mutex protects it).

## Testing

You can run tests locally with `./mach test --auto <path>`, which picks the
right harness automatically. For Web Platform Tests specifically, use
`./mach wpt --headless <path>`. Add `--headless` to any test command to avoid
opening a browser window.

- **Cover workers.** If a feature is exposed to workers, write tests for that
  context. Behaviour may differ from window contexts (e.g. bfcache
  interactions).
- **Use `assert_equals` / `assert_true` / `assert_false`** in Web Platform
  Tests for clear assertions. Prefer `assert_false(value)` over
  `assert_not_equals(value, true)` for boolean checks.
- **Run tests before submitting.** Use `./mach test --auto` on the files you
  changed. Once satisfied locally, use `./mach try auto` to run tests in CI.
