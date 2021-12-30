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

def indent(indent, s):
    if indent == "":
        return s
    if not s:
        return s
    return "\n".join([indent + x for x in s.split('\n')])

def declare_enum(e):
    r = [f"""enum class {e.name}: {numeric(e.type)}""", "{"]
    for n,v in sorted(e.items(), key=lambda t: (t[1], t[0])):
        r += [f"\t{n} = {v},"]
    r += ["};", ""]
    return "\n".join(r)
