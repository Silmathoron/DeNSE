#!/usr/bin/env python
#-*- coding:utf-8 -*-

""" Testing the areas to reproduce Jordi's experiments """

import numpy as np

import nngt
# ~ import nngt.geometry as geom
import PyNCulture as geom
import NetGrowth as ng


'''
Creating the environment: a disk and a weird structure
'''

fname = "mask_high.svg"

shape = geom.Shape.disk(radius=3000)
masks = geom.shapes_from_file(fname, min_x=-2950, max_x=2600)

for i, m in enumerate(masks):
    shape.add_area(m, height=30., name="top_{}".format(i+1))

#~ geom.plot_shape(shape, show=False)


'''
Set the environment in NetGrowth, then create the neurons
'''

num_omp = 12

ng.SetKernelStatus({
    "resolution": 50., "num_local_threads": num_omp,
    "seeds": [i for i in range(num_omp)],
    # ~ "seeds": [11, 6, 7, 9],
})

np.random.seed(1)

ng.SetEnvironment(shape)

shape = ng.GetEnvironment()

# ~ for a in shape.areas.values():
    # ~ geom.plot_shape(a, show=False)
# ~ geom.plot_shape(shape, show=True)

# seed the neurons on top
top_areas = [k for k in shape.areas.keys() if k.find("default_area") != 0]

params = {
    "sensing_angle": 0.04,
    "filopodia_wall_affinity": 2.5,
    "proba_down_move": 0.05,
    "scale_up_move": 5.,
}

# ~ ng.CreateNeurons(n=100, on_area=top_areas, num_neurites=2)
ng.CreateNeurons(n=2000, on_area=top_areas, num_neurites=1, params=params)
# ~ ng.CreateNeurons(n=1, on_area="default_area", num_neurites=1, params=params)

ng.Simulate(2500)
# ~ for i in range(50):
    # ~ ng.Simulate(500)
    # ~ ng.PlotNeuron(show=True)

from NetGrowth.tools import fraction_neurites_near_walls
print(fraction_neurites_near_walls(0, culture=shape))
ng.PlotNeuron(show=True)