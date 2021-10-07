#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import sys
import yaml

input = open(sys.argv[1] if len(sys.argv) > 1 else 'python/test/decimal128.yaml')
output = open(sys.argv[2], 'w') if len(sys.argv) > 2 else sys.stdout

data = yaml.safe_load(input)

special = {'Inf': 'INFINITY', '-Inf': '-INFINITY', 'NaN': 'NAN', 'sNaN': 'NAN'}

for k,v in data.items():
    m = v['significand']
    if m >= 2 ** 64:
        output.write(f"__uint128_t {k}_mantissa = u128_build_18({m // (10 ** 18)}, {m % (10 ** 18)});\n");
        m = f"{k}_mantissa";
    else:
        m = f"{m}ull";

    binary = ''.join([f'\\x{i:02x}' for i in v['bin']])

    s = v['string']
    if s in special:
        s = special[s]
    else:
        s = f'{s}DL'

    output.write(f"""CHECK_D128("{v['string']}", "{binary}", {s}, {v['sign']}, {m}, {v['exponent']});\n""")
