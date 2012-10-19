#!/usr/bin/env python

from kvdata import KeyValueData
from plotlib import PlotLib
import sys
import os

#### EAF-size variation data ####

eaffrac = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6]
efcore1 = [  1,   2,   2,   4,   5,   5,   5,   4,   4]
efcore2 = [ 10,  11,  13,  14,  15,  15,  15,  15,  15]
efcore4 = [ 12,  15,  18,  19,  20,  21,  21,  20,  20]

efdata = [zip(eaffrac, efcore1), zip(eaffrac, efcore2), zip(eaffrac, efcore4)]
efxlabel = 'Size of EAF / Number of Blocks in Cache'
efylabel = '\% Weighted Speedup Improvement over LRU'
eflegend = ['1-Core', '2-Core', '4-Core']
efystep = 3

efplotname = 'eaf-size'

#################################

#### Bloom filter size variation data ####

bffrac =  [  1,    2,    3,    4,    5,    6]
bfcore1 = [  3,    3,    4,    5,    5,    5]
bfcore2 = [  7,    9,   10,   14,   15,   15]
bfcore4 = [ 13,   17,   20,   21,   21,   21]

bfdata = [zip(bffrac, bfcore1), zip(bffrac, bfcore2), zip(bffrac, bfcore4)]
bfxtics = [0.18, 0.36, 0.73, 1.47, 2.94, 5.88]
bfxlabel = 'Percentage Overhead of Bloom Filter vs Cache Size'
bfylabel = '\% Weighted Speedup Improvement over LRU'
bflegend = ['1-Core', '2-Core', '4-Core']
bfystep = 3

bfplotname = 'bf-size'

#################################

plot = PlotLib()

legend = eflegend
plot.set_y_tics_step(efystep)
plot.set_y_tics_shift(0)
plot.set_legend_shift(-50,-10)
plot.set_plot_dimensions(150,80)
plot.set_scale(0.5)
plot.set_y_tics_font_size("scriptsize")
plot.set_x_tics_font_size("scriptsize")
plot.set_y_label_font_size("small")
plot.set_x_label_font_size("small")
plot.set_ceiling(0.8)

plot.xlabel(efxlabel, yshift = -10, fontsize = "small", options =
"[scale=0.8]")
plot.ylabel(efylabel, xshift = -2, fontsize = "small", options = "[scale=0.8]")

plot.continuous_lines(efdata, legend = legend, xtics = eaffrac, use_markers = 2, x_padding = 5)
                      
#plot.column_stacked_bars(workload_order, [kv.values(name, key_order = workload_order) for name in mechanisms], legend)
plot.save_pdf(efplotname)
plot.save_tikz(efplotname + ".tex")


# plot = PlotLib()

# legend = bflegend
# plot.set_y_tics_step(bfystep)
# plot.set_y_tics_shift(0)
# plot.set_legend_shift(-50,-10)
# plot.set_plot_dimensions(150,80)
# plot.set_scale(0.5)
# plot.set_y_tics_font_size("scriptsize")
# plot.set_x_tics_font_size("scriptsize")
# plot.set_y_label_font_size("small")
# plot.set_x_label_font_size("small")
# plot.set_ceiling(0.8)

# plot.xlabel(bfxlabel, yshift = -10, fontsize = "small", options =
# "[scale=0.8]")
# plot.ylabel(bfylabel, xshift = -2, fontsize = "small", options = "[scale=0.8]")

# plot.continuous_lines(bfdata, legend = legend, xtics = bffrac, use_markers = 2, x_padding = 5)
                      
# #plot.column_stacked_bars(workload_order, [kv.values(name, key_order = workload_order) for name in mechanisms], legend)
# plot.save_pdf(bfplotname)
# plot.save_tikz(bfplotname + ".tex")
