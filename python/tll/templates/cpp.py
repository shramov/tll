#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.scheme as S

def weaktrim(text):
    text = text.lstrip('\n')
    r = text.strip()
    if r == '': return r
    return text

NUMERIC = {
    S.Type.Int8: 'int8_t',
    S.Type.Int16: 'int16_t',
    S.Type.Int32: 'int32_t',
    S.Type.Int64: 'int64_t',
    S.Type.UInt8: 'uint8_t',
    S.Type.UInt16: 'uint16_t',
    S.Type.UInt32: 'uint32_t',
    S.Type.UInt64: 'uint64_t',
    S.Type.Double: 'double',
}

def numeric(t):
    return NUMERIC.get(t, None)

RESOLUTION = {
    S.chrono.Resolution.ns: 'std::nano',
    S.chrono.Resolution.us: 'std::micro',
    S.chrono.Resolution.ms: 'std::milli',
    S.chrono.Resolution.second: 'std::ratio<1>',
    S.chrono.Resolution.minute: 'std::ratio<60>',
    S.chrono.Resolution.hour: 'std::ratio<3600>',
    S.chrono.Resolution.day: 'std::ratio<86400>',
}

def time_resolution(f):
    r = RESOLUTION.get(f.time_resolution, None)
    if r is None:
        raise ValueError(f"Unknown time resolution for field {f.name}: {f.time_resolution}")
    return r

OFFSET_PTR_VERSION = {
    S.OffsetPtrVersion.Default: 'tll_scheme_offset_ptr_t',
    S.OffsetPtrVersion.LegacyShort: 'tll_scheme_offset_ptr_legacy_short_t',
    S.OffsetPtrVersion.LegacyLong: 'tll_scheme_offset_ptr_legacy_long_t',
}

def offset_ptr_version(f):
    r = OFFSET_PTR_VERSION.get(f.offset_ptr_version, None)
    if r is None:
        raise ValueError(f"Unknown offset ptr version for field {f.name}: {f.offset_ptr_version}")
    return r

def indent_one(indent, s):
    if not s:
        return s
    return indent + s

def indent(indent, s):
    if indent == "":
        return s
    return "\n".join([indent_one(indent, x) for x in s.split('\n')])

def indent_filter(text):
    return indent('\t', text)
#lambda s: indent(ind', s)

def declare_enum(e):
    r = [f"""enum class {e.name}: {numeric(e.type)}""", "{"]
    for n,v in sorted(e.items(), key=lambda t: (t[1], t[0])):
        r += [f"\t{n} = {v},"]
    r += ["};"]
    return "\n".join(r)

def declare_bits(b):
    r = [f"""struct {b.name}: public tll::scheme::Bits<{numeric(b.type)}>
{{
\tusing tll::scheme::Bits<{numeric(b.type)}>::Bits;"""]
    for f in b.values():
        ft = "unsigned" if f.size > 1 else "bool"
        r += [f"""\tconstexpr auto {f.name}() const {{ return get({f.offset}, {f.size}); }};
\tconstexpr {b.name} & {f.name}({ft} v) {{ set({f.offset}, {f.size}, v); return *this; }};"""]
    r += [f"""\tstatic std::map<std::string_view, value_type> bits_descriptor()
\t{{
\t\treturn {{"""]
    for f in b.values():
        r += [f"""\t\t\t{{ "{f.name}", static_cast<value_type>(Bits::mask({f.size})) << {f.offset} }},"""]
    r += ["""\t\t};
\t}
};"""]
    return "\n".join(r)
