#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import argparse
import logging, logging.config
import signal
import threading

import tll.logger

from tll.config import Config, Url
from tll.channel import Context
from tll.processor import *

parser = argparse.ArgumentParser(description="Run TLL processor")
parser.add_argument("config", type=str, help="configuration file")

def main():
    tll.logger.init()

    args = parser.parse_args()
    if '://' not in args.config:
        args.config = 'yaml://' + args.config
    cfg = Config.load(args.config)
    cfg.process_imports('processor.include')

    try:
        tll.logger.pyconfigure(cfg.sub('logger', throw=False))
    finally:
        logging.basicConfig(level=logging.DEBUG, format='%(asctime)s %(levelname)-7s %(name)s: %(message)s')

    context = Context()
    context.register_loader()

    loader = None
    mcfg = cfg.sub("processor.module", throw=False)
    if mcfg is not None:
        lurl = Url()
        lurl.proto = "loader"
        lurl['tll.internal'] = 'yes'
        lurl['module'] = mcfg.copy()
        loader = context.Channel(lurl)

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
