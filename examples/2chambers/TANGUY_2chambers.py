#!/usr/bin/env python
#-*- coding:utf-8 -*-

import os
import shutil
import time

import numpy as np
import matplotlib.pyplot as plt

import nngt

import dense as ds
from dense.units import *

def CleanFolder(tmp_dir, make=True):
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir)
    if make:
        os.mkdir(tmp_dir)
    return tmp_dir


current_dir = os.path.abspath(os.path.dirname(__file__))
main_dir = current_dir[:current_dir.rfind("/")]


'''
Main parameters
'''

soma_radius = 10.*um
culture_size = 900.
use_uniform_branching = False
use_vp = False
use_run_tumble = False

gc_model = 'persistent_random_walk'

neuron_params = {
    "growth_cone_model": gc_model,
    "use_uniform_branching": use_uniform_branching,
    "use_van_pelt": use_vp,
    "sensing_angle": 0.08 * rad,
    "speed_growth_cone": 0.95 * um / minute,
    "filopodia_wall_affinity": 20.,
    "filopodia_finger_length": 30. * um,
    "filopodia_min_number": 30,

    "soma_radius": soma_radius,
}

dendrite_params = {
    "use_van_pelt": True,
    "growth_cone_model": gc_model,
    "speed_growth_cone": 0.1 * um/ minute,
    "filopodia_wall_affinity": 0.00,
    "persistence_length" : 20. * um
}


'''
Check for optional parameters
'''

if use_run_tumble:
    neuron_params ={
        "persistence_length":12. * um
    }

if use_uniform_branching:
    neuron_params["uniform_branching_rate"] = 0.001 * cpm


if neuron_params.get("growth_cone_model", "") == "persistent_random_walk":
    neuron_params["persistence_length"] = 20. * um


'''
Simulation
'''


def step(n, loop_n, plot=True):
    ds.Simulate(n)
    if plot:
        ds.PlotNeuron(show_nodes=True, show=True)


if __name__ == '__main__':
    #~ kernel={"seeds":[33, 64, 84, 65],
            #~ "num_local_threads":4,
            #~ "resolution": 30.}
    kernel = {"seeds": [33, 64, 84, 65, 68, 23],
              "num_local_threads": 6,
              "resolution": 10. * minute}
    # ~ kernel={"seeds":[33],
    # ~ "num_local_threads": 1,
    # ~ "resolution": 30.}
    #~ kernel={"seeds":[23, 68],
    #~ "num_local_threads": 2,
    #~ "resolution": 30.}
    kernel["environment_required"] = True

    culture_file = current_dir + "/2chamber_culture_sharpen.svg"
    ds.SetKernelStatus(kernel, simulation_ID="ID")
    gids, culture = None, None

    num_neurons = 10

    if kernel["environment_required"]:
        culture = ds.SetEnvironment(culture_file, min_x=-culture_size, max_x=culture_size)
        # generate the neurons inside the left chamber
        pos_left = culture.seed_neurons(
            neurons=num_neurons, xmax=-220*um, soma_radius=soma_radius)
        pos_right = culture.seed_neurons(
            neurons=num_neurons, xmin=680*um, soma_radius=soma_radius)
        neuron_params['position'] = np.concatenate((pos_right, pos_left))*um
    else:
        neuron_params['position'] = np.random.uniform(
            -1000, 1000, (int(2*num_neurons), 2))

    print("Creating neurons")
    gids = ds.CreateNeurons(n=int(2*num_neurons),
                            growth_cone_model="persistent_rw_critical",
                            culture=culture,
                            params=neuron_params,
                            dendrites_params=dendrite_params,
                            num_neurites=2)

    start = time.time()
    step(4000*minute, 0, False)

    dendrite_params.update({"speed_growth_cone" : 0.001*um/minute,
                            "use_van_pelt" : False})

    axon_params = {"speed_growth_cone" : 0.7*um/minute,
                            "use_van_pelt" : True,
                   'B' : 10.*cpm,
                   'T' : 10000.*minute,
                   'E' : 0.7}
    print(time.time() - start)
    ds.SetStatus(gids, params=neuron_params, dendrites_params=dendrite_params,
                 axon_params=axon_params)
    print(time.time() - start)

    print("SIMULATION STARTED")
    step(2000*minute, 0, False)
    print("SIMULATION ENDED")
    # ~ for loop_n in range(5):
    # ~ step(500, loop_n, True)
    duration = time.time() - start
    print(duration)

    # prepare the plot
    fig, ax = plt.subplots()
    ds.plot.PlotNeuron(gid=range(num_neurons), culture=culture, soma_alpha=0.8,
                       axon_color='g', gc_color="r", axis=ax, show=False)
    ds.plot.PlotNeuron(gid=range(num_neurons, 2*num_neurons), show_culture=False,
                       axis=ax, soma_alpha=0.8, axon_color='darkorange',
                       gc_color="r", show=False)

    # save
    save_path = CleanFolder(os.path.join(os.getcwd(), "2culture_swc"))
    ds.SaveJson(filepath=save_path)
    ds.SaveSwc(filepath=save_path, swc_resolution=10)

    graph = ds.CreateGraph()

    nngt.plot.draw_network(graph, show=True)
