#pragma once

#include <tll/scheme/types.h>

#pragma pack(push, 1)
% if options.namespace:
namespace ${options.namespace} {
% endif
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

def field2type(f):
    t = numeric(f.type)
    if t is not None:
	if f.sub_type == f.Sub.Bits:
	    #return f"tll::scheme::Bits<{t}>"
	    return f.name
	elif f.sub_type == f.Sub.Enum:
	    return f.type_enum.name
        return t
    elif f.type == f.Decimal128:
        return "std::array<char, 16>"
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
        return f"tll::scheme::offset_ptr_t<{t}>"
    raise ValueError(f"Unknown type for field {f.name}: {f.type}")

def enum2decl(e):
    t = numeric(e.type)
    body = ", ".join([f"{n} = {v}" for n,v in e.items()])
    return f"enum class {e.name}: {t} {{{body}}}"

def field2decl(f):
    if f.type == f.Array:
        return field2decl(f.type_array)
    elif f.type == f.Pointer:
        return field2decl(f.type_ptr)

    if f.sub_type == f.Sub.Bits:
	t = numeric(f.type)
	body = []
	for n,b in sorted(f.bitfields.items()):
	    bt = "unsigned" if b.size > 1 else "bool"
	    body.append(f"""\t\tauto {b.name}() const {{ return get({b.offset}, {b.size}); }}; void {b.name}({bt} v) {{ return set({b.offset}, {b.size}, v); }};
""")
	body = "".join(body)
	return f"""struct {f.name}: public tll::scheme::Bits<{t}>
	{{
{body}	}};"""
%>
% for e in scheme.enums.values():
${enum2decl(e)};

% endfor
% for msg in scheme.messages:
struct ${ msg.name } {
% if msg.msgid != 0:
	static constexpr int ${options.msgid} = ${msg.msgid};
% endif
% for e in msg.enums.values():
	${enum2decl(e)};
% endfor
% for f in msg.fields:
% if field2decl(f):
	${field2decl(f)}
% endif
	${field2type(f)} ${f.name};
% endfor
};

% endfor
% if options.namespace:
} // namespace ${options.namespace}
% endif
#pragma pack(pop)
