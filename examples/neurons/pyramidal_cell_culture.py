#!/usr/bin/env python
# -*- coding:utf-8 -*-

""" Generate the morphology of a pyramidal cell in culture """

import dense as ds
import numpy as np
import os

import matplotlib.pyplot as plt
import seaborn as sns


# parameters

num_omp     = 1
num_neurons = 1
filename    = "pyramidal-cell.swc"

pyr_nrn = {
    "dendrite_diameter": 3.,
    "axon_diameter": 4.,
    "polarization_strength": 20.,
    "neurite_angles": {"axon": 1.57, "dendrite_1": 3.9, "dendrite_2": 5.5},
    "description": "pyramidal cell",
    "soma_radius": 8.,
    "position": [(0., 0.)]
}

# initial period (first 5 days)

pyr_axon_i = {
    "persistence_length": 500.,
    "speed_growth_cone": 0.07,
    # diameter
    "thinning_ratio": 1./320.,
    "diameter_ratio_avg": 0.5,
    # branching
    "use_van_pelt": False,
    "use_uniform_branching": False,
}

pyr_dend_i = {
    "persistence_length": 250.,
    "speed_growth_cone": 0.01,
    "thinning_ratio": 1./200.,
    "use_uniform_branching": False,
    "use_van_pelt": True,
    "B": 1.,
    "T": 5000.,
    "gc_split_angle_mean": 25.,
}

# branching period (next 15 days)

pyr_axon_lb = {
    "speed_growth_cone": 0.025,
    "use_van_pelt": False,
    "use_flpl_branching": True,
    "flpl_branching_rate": 0.0006,
    "speed_growth_cone": 0.02,
    "lateral_branching_angle_mean": 45.,
}

pyr_dend_lb = {
    "thinning_ratio": 1./100.,
    "use_van_pelt": False,
    "use_uniform_branching": True,
    "uniform_branching_rate": 0.0003,
    "persistence_length": 100.,
    "speed_growth_cone": 0.01,
    "lateral_branching_angle_mean": 40.,
}

# termination period (next 10 days)

axon_t = {
    "speed_growth_cone": 0.015,
    "use_flpl_branching": False}

dend_t = {
    "use_van_pelt": True,
    "use_uniform_branching": False,
    "B": 5.,
    "T": 50000.,
    "gc_split_angle_mean": 30.,
}


def show_nrn(neuron):
    ds.NeuronToSWC(filename, gid=neuron)
    import neurom as nm
    from neurom import viewer
    nrn = nm.load_neuron(filename)

    fig, ax = viewer.draw(nrn)
    plt.axis('off')
    fig.suptitle("")
    ax.set_title("")
    plt.tight_layout()
    plt.show()


''' Init kernel and create neurons '''

kernel = {
    "resolution": 50.,
    "seeds": [1],
    "environment_required": False,
    "num_local_threads": num_omp,
    "adaptive_timestep": -1.,
}

ds.SetKernelStatus(kernel)

pyr_neuron = ds.CreateNeurons(1, params=pyr_nrn, axon_params=pyr_axon_i,
                              dendrites_params=pyr_dend_i, num_neurites=3)

# initial extension (5 days)

ds.Simulate(7200)
show_nrn(pyr_neuron)

# Extension and branching period (5 days)

ds.SetStatus(pyr_neuron, axon_params=pyr_axon_lb, dendrites_params=pyr_dend_lb)

ds.Simulate(7200)
show_nrn(pyr_neuron)

# Extension and branching period (5 more days)

ds.Simulate(7200)
show_nrn(pyr_neuron)

# Termination period (10 days)

ds.SetStatus(pyr_neuron, axon_params=axon_t, dendrites_params=dend_t)

ds.Simulate(14400)
show_nrn(pyr_neuron)
