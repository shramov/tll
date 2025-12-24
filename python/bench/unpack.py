#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import timeit

import tll.logger
tll.logger.init()

import tll.scheme as S

scheme = '''
- name: Enum
  fields:
    - {name: e8, type: int8, options.type: enum, enum: {A: 0, B: 1, C: 2, D: 3, E: 4, F: 5, G: 6, H: 7, I: 8, J: 9}}
- name: KV
  fields:
    - {name: key, type: string}
    - {name: value, type: string}
- name: List
  fields:
    - {name: header, type: int64}
    - {name: list, type: '*KV'}
- name: Scalar
  fields:
    - {name: i8,  type: int8}
    - {name: i16, type: int16}
    - {name: i32, type: int32}
    - {name: i64, type: int64}
    - {name: u8,  type: uint8}
    - {name: u16, type: uint16}
    - {name: u32, type: uint32}
    - {name: u64, type: uint64}
    - {name: f64, type: double}
'''
scheme = S.Scheme('yamls://' + scheme)

benchmarks = {
    'List': {
        'header': 0xbeef,
        'list': [
            {'key': 'key0', 'value': 'value0'},
            {'key': 'key1', 'value': 'value1'},
            {'key': 'key2', 'value': 'value2'},
            {'key': 'key3', 'value': 'value3'},
            {'key': 'key4', 'value': 'value4'},
        ]
    },
    'Scalar': {
        'i8': -10, 'i16': -1000, 'i32': -1000000, 'i64': -10000000000,
        'u8': 10, 'u16': 1000, 'u32': 1000000, 'u64': 10000000000,
        'f64': 1234.5678
    },
    'Enum': {'e8': 'F'},
}

def run_test(name, data, count = 100000):
    msg = scheme[name]
    body = msg(**data).pack()
    r = timeit.timeit(lambda: msg.unpack(memoryview(body)), number=count)
    print(f'{name:>10}: {r:5.3}s, {r / count * 1000000:5.3}us/msg')

# Prewarm
for i in range(100000):
    i = i * i

for k, v in sorted(benchmarks.items()):
    run_test(k, v)
