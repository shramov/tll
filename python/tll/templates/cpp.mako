<%def name='setup_options(parser)'><%
    parser.add_argument('--namespace', dest='namespace', type=str,
                        help='C++ namespace name for generated source')
    parser.add_argument('--info-struct', dest='info_struct', type=str, default='tll_message_info',
                        help='Name of message info structure (default %(default)s)')
%></%def>\
<%!
from tll.templates import cpp
%>\
#pragma once

#include <tll/scheme/types.h>

#pragma pack(push, 1)
% if options.namespace:
namespace ${options.namespace} {
% endif

template <typename T>
struct tll_message_info {};

static constexpr std::string_view scheme_string = R"(${scheme.dump('yamls+gz')})";
<%
def field2type(f):
    t = cpp.numeric(f.type)
    if t is not None:
	if f.sub_type == f.Sub.Bits:
	    #return f"tll::scheme::Bits<{t}>"
	    return f.name
	elif f.sub_type == f.Sub.Enum:
	    return f.type_enum.name
	elif f.sub_type == f.Sub.FixedPoint:
	    return f"tll::scheme::FixedPoint<{t}, {f.fixed_precision}>";
	elif f.sub_type == f.Sub.Duration:
	    return f"std::chrono::duration<{t}, {cpp.time_resolution(f)}>";
	elif f.sub_type == f.Sub.TimePoint:
	    return f"std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<{t}, {cpp.time_resolution(f)}>>";
        return t
    elif f.type == f.Decimal128:
        return "tll::scheme::Decimal128"
    elif f.type == f.Bytes:
	if f.sub_type == f.Sub.ByteString:
	    return f"tll::scheme::ByteString<{f.size}>"
        return f"tll::scheme::Bytes<{f.size}>"
    elif f.type == f.Message:
        return f.type_msg.name
    elif f.type == f.Union:
        return f"struct {f.type_union.name}"
    elif f.type == f.Array:
    	t = field2type(f.type_array)
	ct = field2type(f.count_ptr)
        return f"tll::scheme::Array<{t}, {f.count}, {ct}>"
    elif f.type == f.Pointer:
    	t = field2type(f.type_ptr)
	if f.sub_type == f.Sub.ByteString:
	    return f'tll::scheme::String<{cpp.offset_ptr_version(f)}>'
        return f"tll::scheme::offset_ptr_t<{t}, {cpp.offset_ptr_version(f)}>"
    raise ValueError(f"Unknown type for field {f.name}: {f.type}")
%>\
<%def name='enum2code(e)'>\
	enum class ${e.name}: ${cpp.numeric(e.type)}
	{
% for n,v in sorted(e.items(), key=lambda t: (t[1], t[0])):
		${n} = ${v},
% endfor
	};
</%def>\
<%def name='union2decl(u)'>\
	struct ${u.name}: public tll::scheme::UnionBase<${cpp.numeric(u.type_ptr.type)}, ${u.union_size}>
	{
% for uf in u.fields:
<% t = field2type(uf) %>\
		${t} * get_${uf.name}() { return getT<${t}>(${uf.union_index}); }
		const ${t} * get_${uf.name}() const { return getT<${t}>(${uf.union_index}); }
		${t} & unchecked_${uf.name}() { return uncheckedT<${t}>(); }
		const ${t} & unchecked_${uf.name}() const { return uncheckedT<${t}>(); }

% endfor
	};
</%def>\
<%def name='field2code(f)'>\
	${field2type(f)} ${f.name};\
</%def>
% for e in scheme.enums.values():
${cpp.declare_enum(e)}
% endfor
% for u in scheme.unions.values():
<%call expr='union2decl(u)'></%call>
% endfor
% for b in scheme.bits.values():
${cpp.declare_bits(b)}
% endfor
% for msg in scheme.messages:
struct ${msg.name}
{
% for e in msg.enums.values():
${cpp.indent("\t", cpp.declare_enum(e))}
% endfor
% for u in msg.unions.values():
<%call expr='union2decl(u)'></%call>
% endfor
% for b in msg.bits.values():
${cpp.indent("\t", cpp.declare_bits(b))}
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
