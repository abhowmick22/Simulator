#!/usr/bin/env python

import os

mapping = {
    '0': 'L',
    '1': 'L',
    '2': 'M',
    '3': 'H',
    '4': 'H',
    }

for cb, cb_map in mapping.items():
    for pb, pb_map in mapping.items():
        os.system("cat fine/%s-cb.%s-pb >> course/%s-cb.%s-pb" % (cb, pb, cb_map, pb_map))
