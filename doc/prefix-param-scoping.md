# Prefix channels: parameters, aliases and `sub.` — what actually exists

This note records facts established from the sources and live runs on the question "how to separate two layers of the same prefix channel in one chain" (for example `lua+patch+lua+file`, where the outer `lua+` is a filter, the inner is a transform).

## How a prefix reads its parameters

Each channel reads its settings via `channel_props_reader`, and the key prefix is taken from `param_prefix()`, which is **derived from `channel_protocol()` at compile time** — `"lua+"` minus `+` equals `"lua"`:

- `src/tll/channel/base.h:92-101` — `param_prefix()`.
- `src/tll/channel/base.h:200-210` — `channel_props_reader()` chain: `props.sub(param_prefix)` → `props` → `config_defaults.sub(param_prefix)`.
- `../tll-lua/src/prefix.h:39` — `LuaPrefix::channel_protocol() = "lua+"`, so its parameters are read as `lua.code`, `lua.preset`, etc.

Consequence: **all** `lua+` layers share the same prefix — `lua`. Any flat key `lua.code=…` is visible to every layer in the chain.

## Aliases do not change the parameter prefix

`Context::alias(name, url)` registers a prototype under a different name, but:

1. resolution reduces the alias to the **same** class (`Lookup prefix` → "Found alias" → "Initialize channel with impl '…'"), and `channel_protocol()`/`param_prefix()` are compile-time constants of the class. The registry name does not affect them.
2. alias parameters are merged into the url using **flat** keys:
```cpp
// src/channel/context.cc:374-379
for (auto &[k, v]: alias.browse("**")) {
    if (k == "tll.proto" || k == "tll.host") continue;
    if (url.has(k))
        return error(fmt::format("Duplicate field '{}': both in alias '{}' and in url", k, alias.proto()));
    url.set(k, *v.get());
}
```
That is, `lua.preset=filter` embedded in an alias becomes a flat `lua.preset`, which the inner layer will also read; a key collision with the url produces a `Duplicate field '…'` error (confirmed by live runs).

Test to verify the behavior: `python/test/test_channel.py::test_alias`.

## There is no `sub.`/`sub.sub.` mechanism in master

There is no scoping mechanism to "pass a parameter to the N-th level of the chain" via `sub.`, `sub.sub.` keys in current TLL. The only mention in history is a commented sketch in the `wip-config` branch:

```cpp
// origin/wip-config: src/tll/channel/prefix.h:49
auto sub = curl.sub("sub");
if (sub) { curl.detach(sub); curl.merge(sub, true); }
```

It did not land in master, is not documented anywhere, and does not work.

## Conclusion

For two layers of the same prefix channel in one chain to have **different** settings, they need **different `param_prefix`**, which is only possible via a separate `channel_protocol()` — that is, a separate channel class (e.g. `lua-filter+` alongside `lua+`). Aliases and `sub.` do not provide this.

An alternative without introducing new classes is to not run the second layer as a channel prefix: perform type selection with the standard `filter+` (`messages=A,B,!C`), and evaluate the expression with a separate eval (like `lua+direct`), not a `lua+` prefix.
