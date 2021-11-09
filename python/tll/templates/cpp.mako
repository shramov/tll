<%def name='setup_options(parser)'><%
    parser.add_argument('--namespace', dest='namespace', type=str,
                        help='C++ namespace name for generated source')
    parser.add_argument('--info-struct', dest='info_struct', type=str, default='tll_message_info',
                        help='Name of message info structure (default %(default)s)')
%></%def>\
#pragma once

#include <tll/scheme/types.h>

#pragma pack(push, 1)
% if options.namespace:
namespace ${options.namespace} {
% endif

template <typename T>
struct tll_message_info {};

static constexpr std::string_view scheme_string = R"(${scheme.dump('yamls+gz')})";
<%!
def weaktrim(text):
    text = text.lstrip('\n')
    r = text.strip()
    if r == '': return r
    return text
%>\
<%
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

def field2type(f):
    t = numeric(f.type)
    if t is not None:
	if f.sub_type == f.Sub.Bits:
	    #return f"tll::scheme::Bits<{t}>"
	    return f.name
	elif f.sub_type == f.Sub.Enum:
	    return f.type_enum.name
	elif f.sub_type == f.Sub.FixedPoint:
	    return f"tll::scheme::FixedPoint<{t}, {f.fixed_precision}>";
	elif f.sub_type == f.Sub.Duration:
	    return f"std::chrono::duration<{t}, {time_resolution(f)}>";
	elif f.sub_type == f.Sub.TimePoint:
	    return f"std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<{t}, {time_resolution(f)}>>";
        return t
    elif f.type == f.Decimal128:
        return "tll::scheme::Decimal128"
    elif f.type == f.Bytes:
	if f.sub_type == f.Sub.ByteString:
	    return f"tll::scheme::ByteString<{f.size}>"
        return f"tll::scheme::Bytes<{f.size}>"
    elif f.type == f.Message:
        return f.type_msg.name
    elif f.type == f.Array:
    	t = field2type(f.type_array)
	ct = field2type(f.count_ptr)
        return f"tll::scheme::Array<{t}, {f.count}, {ct}>"
    elif f.type == f.Pointer:
    	t = field2type(f.type_ptr)
        return f"tll::scheme::offset_ptr_t<{t}, {OFFSET_PTR_VERSION[f.offset_ptr_version]}>"
    raise ValueError(f"Unknown type for field {f.name}: {f.type}")
%>\
<%def name='enum2code(e)'>\
	enum class ${e.name}: ${numeric(e.type)}
	{
% for n,v in sorted(e.items(), key=lambda t: (t[1], t[0])):
		${n} = ${v},
% endfor
	};
</%def>\
<%def name='field2decl(f)' filter='weaktrim'>
% if f.type == f.Array:
<%call expr='field2decl(f.type_array)'></%call>\
% elif f.type == f.Pointer:
<%call expr='field2decl(f.type_ptr)'></%call>\
% elif f.sub_type == f.Sub.Bits:
	struct ${f.name}: public tll::scheme::Bits<${numeric(f.type)}>
	{
		using tll::scheme::Bits<${numeric(f.type)}>::Bits;
% for n,b in sorted(f.bitfields.items(), key=lambda t: (t[1].offset, t[1].size, t[0])):
		constexpr auto ${b.name}() const { return get(${b.offset}, ${b.size}); }; constexpr ${f.name} & ${b.name}(${"unsigned" if b.size > 1 else "bool"} v) { set(${b.offset}, ${b.size}, v); return *this; };
% endfor
		static std::map<std::string_view, value_type> bits_descriptor()
		{
			return {
% for n,b in sorted(f.bitfields.items(), key=lambda t: (t[1].offset, t[1].size, t[0])):
				{ "${b.name}", static_cast<value_type>(Bits::mask(${b.size})) << ${b.offset} },
% endfor
			};
		}
	};
% endif
</%def>\
<%def name='field2code(f)'>\
<%call expr='field2decl(f)'></%call>\
	${field2type(f)} ${f.name};\
</%def>
% for e in scheme.enums.values():
<%call expr='enum2code(e)'></%call>
% endfor
% for msg in scheme.messages:
struct ${msg.name}
{
% for e in msg.enums.values():
<%call expr='enum2code(e)'></%call>
% endfor
% for f in msg.fields:
<%call expr='field2code(f)'></%call>
% endfor
};

template <>
struct tll_message_info<${msg.name}>
{
% if msg.msgid != 0:
	static constexpr int id = ${msg.msgid};
% endif
	static constexpr std::string_view name = "${msg.name}";
};
% endfor
% if options.namespace:
} // namespace ${options.namespace}
% endif
#pragma pack(pop)
