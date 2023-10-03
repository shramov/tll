#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import argparse
import logging, logging.config
import signal
import sys
import threading

import tll.logger

from tll.config import Config, Url
from tll.channel import Context
from tll.processor import *

parser = argparse.ArgumentParser(description="Run TLL processor")
parser.add_argument("config", type=str, help="configuration file")
parser.add_argument("-D", dest='defs', metavar='KEY=VALUE', action='append', default=[], help='extra config variables')

def main():
    tll.logger.init()

    args = parser.parse_args()
    if '://' not in args.config:
        args.config = 'yaml://' + args.config
    cfg = Config.load(args.config)
    for d in args.defs:
        kv = d.split('=', 1)
        if len(kv) != 2:
            print(f"Invalid KEY=VALUE parameter: '{d}'")
            sys.exit(1)
        cfg[kv[0]] = kv[1]
    cfg.process_imports('processor.include')

    return run(cfg)

def run(cfg):
    try:
        tll.logger.configure(cfg.sub('logger', throw=False))
    finally:
        logging.basicConfig(level=logging.DEBUG, format='%(asctime)s %(levelname)-7s %(name)s: %(message)s')

    context = Context(cfg.sub("processor.defaults", create=False, throw=False) or Config())

    Processor.load_modules(context, cfg)

    p = Processor(cfg, context=context)
    p.open()

    signal.signal(signal.SIGINT, lambda *a: p.close())
    signal.signal(signal.SIGTERM, lambda *a: p.close())

    threads = []
    for w in p.workers:
        def run(worker):
            worker.open()
            worker.run()
        t = threading.Thread(target = run, args=(w,))
        t.start()
        threads.append(t)

    p.run()

if __name__ == "__main__":
    main()
