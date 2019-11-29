# -*- coding: utf-8 -*-
#
# plot_structures.py
#
# This file is part of DeNSE.
#
# Copyright (C) 2019 SeNEC Initiative
#
# DeNSE is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# DeNSE is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with DeNSE. If not, see <http://www.gnu.org/licenses/>.

""" Plot recording """

from collections import deque

from matplotlib.cm import get_cmap
from matplotlib.patches import Rectangle
from matplotlib.patches import PathPatch
from matplotlib.textpath import TextPath

import numpy as np

from .. import _pygrowth as _pg
from .._helpers import is_iterable
from ..environment import plot_shape
from ..units import *
from .plot_utils import *


# ------------ #
# Plot neurons #
# ------------ #

def plot_neurons(gid=None, mode="sticks", show_nodes=False, show_active_gc=True,
                 culture=None, show_culture=True, aspect=1., soma_radius=None,
                 active_gc="d", gc_size=2., soma_color='k', scale=50*um,
                 scale_text=True, axon_color="indianred",
                 dendrite_color="royalblue", subsample=1, save_path=None,
                 title=None, axis=None, show_density=False, dstep=20.,
                 dmin=None, dmax=None, colorbar=True, show_neuron_id=False,
                 show=True, **kwargs):
    '''
    Plot neurons in the network.

    Parameters
    ----------
    gid : int or list, optional (default: all neurons)
        Id(s) of the neuron(s) to plot.
    mode : str, optional (default: "sticks")
        How to draw the neurons. By default, the "sticks" mode shows the real
        width of the neurites. Switching to "lines" only leaves the trajectory
        of the growth cones, without information about the neurite width.
        Eventually, the "mixed" mode shows both informations superimposed.
    culture :  :class:`~dense.environment.Shape`, optional (default: None)
        Shape of the environment; if the environment was already set using
        :func:`~dense.CreateEnvironment`.
    show_nodes : bool, optional (default: False)
        Show the branching nodes.
    show_active_gc : bool, optional (default: True)
        If True, display the tip (growth cone) of actively growing branches.
    show_culture : bool, optional (default: True)
        If True, displays the culture in which the neurons are embedded.
    aspect : float, optional (default: 1.)
        Set the aspect ratio between the `x` and `y` axes.
    soma : str, optional (default: "o")
        Shape of the soma marker using the matplotlib conventions.
    soma_radius : float, optional (default: real neuron radius)
        Size of the soma marker.
    active_gc : str, optional (default: "d")
        Shape of the active growth cone marker using the matplotlib
        conventions.
    gc_size : float, optional (default: 2.)
        Size of the growth cone marker.
    axon_color : valid matplotlib color, optional (default: "indianred")
        Color of the axons.
    dendrite_color : valid matplotlib color, optional (default: "royalblue")
        Color of the dendrites.
    soma_color : valid matplotlib color, optional (default: "k")
        Color of the soma.
    scale : length, optional (default: 50 microns)
        Whether a scale bar should be displayed, with axes hidden. If ``None``,
        then spatial measurements will be given through standard axes.
    subsample : int, optional (default: 1)
        Subsample the neurites to save memory.
    save_path : str, optional (default: not saved)
        Path where the plot should be saved, including the filename, pdf only.
    title : str, optional (default: no title)
        Title of the plot.
    axis : :class:`matplotlib.pyplot.Axes`, optional (default: None)
        Axis on which the plot should be drawn, otherwise a new one will be
        created.
    show_neuron_id : bool, optional (default: False)
        Whether the GID of the neuron should be displayed inside the soma.
    show : bool, optional (default: True)
        Whether the plot should be displayed immediately or not.
    **kwargs : optional arguments
        Details on how to plot the environment, see :func:`plot_environment`.

    Returns
    -------
    axes : axis or tuple of axes if `density` is True.
    '''
    import matplotlib.pyplot as plt

    assert mode in ("lines", "sticks", "mixed"),\
        "Unknown `mode` '" + mode + "'. Accepted values are 'lines', " +\
        "'sticks' or 'mixed'."

    if show_density:
        subsample = 1 

    # plot
    fig, ax, ax2 = None, None, None
    if axis is None:
        fig, ax = plt.subplots()
    else:
        ax = axis
        fig = axis.get_figure()
    new_lines = 0

    # plotting options
    soma_alpha = kwargs.get("soma_alpha", 0.8)
    axon_alpha = kwargs.get("axon_alpha", 0.6)
    dend_alpha = kwargs.get("dend_alpha", 0.6)
    gc_color   = kwargs.get("gc_color", "g")

    # get the objects describing the neurons
    if gid is None:
        gid = _pg.get_neurons(as_ints=True)
    elif not is_iterable(gid):
        gid = [gid]

    somas, growth_cones, nodes = None, None, None
    axon_lines, dend_lines     = None, None
    axons, dendrites           = None, None

    if mode in ("lines", "mixed"):
        somas, axon_lines, dend_lines, growth_cones, nodes = \
            _pg._get_pyskeleton(gid, subsample)
    if mode in ("sticks", "mixed"):
        axons, dendrites, somas = _pg._get_geom_skeleton(gid)

    # get the culture if necessary
    env_required = _pg.get_kernel_status('environment_required')
    if show_culture and env_required:
        if culture is None:
            culture = _pg.get_environment()
        plot_environment(culture, ax=ax, show=False, **kwargs)
        new_lines += 1

    # plot the elements
    if mode in ("sticks", "mixed"):
        for a in axons.values():
            plot_shape(a, axis=ax, fc=axon_color, show_contour=False, zorder=2,
                       alpha=axon_alpha, show=False)

        for vd in dendrites.values():
            for d in vd:
                plot_shape(d, axis=ax, fc=dendrite_color, show_contour=False,
                           alpha=dend_alpha, zorder=2, show=False)

    if mode in ("lines", "mixed"):
        ax.plot(axon_lines[0], axon_lines[1], ls="-", c=axon_color)
        ax.plot(dend_lines[0], dend_lines[1], ls="-", c=dendrite_color)
        new_lines += 2

    # plot the rest if required
    if show_nodes and mode in ("lines", "mixed"):
        ax.plot(nodes[0], nodes[1], ls="", marker="d", ms="1", c="k", zorder=4)
        new_lines += 1

    if show_active_gc and mode in ("lines", "mixed"):
        ax.plot(growth_cones[0], growth_cones[1], ls="", marker=active_gc,
                c=gc_color, ms=gc_size, zorder=4)
        new_lines += 1

    # plot the somas
    n = len(somas[2])
    radii = somas[2] if soma_radius is None else np.repeat(soma_radius, n)

    if mode in ("sticks", "mixed"):
        radii *= 1.05

    r_max = np.max(radii)
    r_min = np.min(radii)

    size  = (1.5*r_min if len(gid) <= 10
             else (r_min if len(gid) <= 100 else 0.7*r_min))

    for i, x, y, r in zip(gid, somas[0], somas[1], radii):
        circle = plt.Circle(
            (x, y), r, color=soma_color, alpha=soma_alpha)
        artist = ax.add_artist(circle)
        artist.set_zorder(5)
        if show_neuron_id:
            str_id    = str(i)
            xoffset   = len(str_id)*0.35*size
            text      = TextPath((x-xoffset, y-0.35*size), str_id, size=size)
            textpatch = PathPatch(text, edgecolor="w", facecolor="w",
                                   linewidth=0.01*size)
            ax.add_artist(textpatch)
            textpatch.set_zorder(6)

    # set the axis limits
    if (not show_culture or not env_required) and len(ax.lines) == new_lines:
        if mode in ("lines", "mixed"):
            _set_ax_lim(ax, axon_lines[0] + dend_lines[0],
                        axon_lines[1] + dend_lines[1], offset=2*r_max)
        else:
            xx = []
            yy = []

            for a in axons.values():
                xmin, ymin, xmax, ymax = a.bounds
                xx.extend((xmin, xmax))
                yy.extend((ymin, ymax))

            for vd in dendrites.values():
                for d in vd:
                    xmin, ymin, xmax, ymax = d.bounds
                    xx.extend((xmin, xmax))
                    yy.extend((ymin, ymax))

            _set_ax_lim(ax, xx, yy, offset=2*r_max)

    ax.set_aspect(aspect)

    if title is not None:
        fig.suptitle(title)

    if save_path is not None:
        if not save_path.endswith('pdf'):
            save_path += ".pdf"
        plt.savefig(save_path, format="pdf", dpi=300)

    if show_density:
        from matplotlib.colors import LogNorm
        fig, ax2 = plt.subplots()
        x = np.concatenate(
            (np.array(axons[0])[~np.isnan(axons[0])],
             np.array(dendrites[0])[~np.isnan(dendrites[0])]))
        y = np.concatenate(
            (np.array(axons[1])[~np.isnan(axons[1])],
             np.array(dendrites[1])[~np.isnan(dendrites[1])]))
        xbins = int((np.max(x) - np.min(x)) / dstep)
        ybins = int((np.max(y) - np.min(y)) / dstep)

        cmap = get_cmap(kwargs.get("cmap", "viridis"))
        cmap.set_bad((0, 0, 0, 1))
        norm = None

        if dmin is not None and dmax is not None:
            n = int(dmax-dmin)
            norm = matplotlib.colors.BoundaryNorm(
                np.arange(dmin-1, dmax+1, 0), cmap.N)
        elif dmax is not None:
            n = int(dmax)
            norm = matplotlib.colors.BoundaryNorm(
                np.arange(0, dmax+1, 1), cmap.N)
            
        counts, xbins, ybins = np.histogram2d(x, y, bins=(xbins, ybins))
        lims = [xbins[0], xbins[-1], ybins[0], ybins[-1]]
        counts[counts == 0] = np.NaN

        data = ax2.imshow(counts.T, extent=lims, origin="lower",
                          vmin=0 if dmin is None else dmin, vmax=dmax,
                          cmap=cmap)
        
        if colorbar:
            extend = "neither"
            if dmin is not None and dmax is not None:
                extend = "both"
            elif dmax is not None:
                extend = "max"
            elif dmin is not None:
                extend = "min"
            cb = plt.colorbar(data, ax=ax2, extend=extend)
            cb.set_label("Number of neurites per bin")
        ax2.set_aspect(aspect)
        ax2.set_xlabel(r"x ($\mu$ m)")
        ax2.set_ylabel(r"y ($\mu$ m)")
    
    if scale is not None:
        xmin, xmax = ax.get_xlim()
        ymin, ymax = ax.get_ylim()

        length = scale.m_as("micrometer")

        if xmax - xmin < 2*length:
            scale *= 0.2
            length = scale.m_as("micrometer")

        x = xmin + 0.2*length
        y = ymin + (ymax-ymin)*0.05

        ax.add_artist(
            Rectangle((x, y), length, 0.1*length, fill=True, facecolor='k',
                      edgecolor='none'))

        plt.axis('off')

        stext = "(scale is {} $\mu$m)".format(length)
        if title is not None and scale_text:
            fig.suptitle(title + " " + stext)
        elif scale_text:
            fig.suptitle(stext)

    if show:
        plt.show()

    if show_density:
        return ax, ax2

    return ax


# --------------- #
# Plot dendrogram #
# --------------- #

def plot_dendrogram(neurite, axis=None, show=True, **kwargs):
    '''
    Plot the dendrogram of a neurite.

    Parameters
    ----------
    neurite : :class:`~dense.elements.Neurite` object
        Neurite for which the dendrogram should be plotted.
    axis : matplotlib.Axes.axis object, optional (default: new one)
        Axis on which the dendrogram should be plotted.
    show : bool, optional (default: True)
        Whether the figure should be shown right away.
    **kwargs : arguments for :class:`matplotlib.patches.Rectangle`
        For instance `facecolor` or `edgecolor`.
    '''
    tree     = neurite.get_tree()
    branches = neurite.branches

    if axis is None:
        fig, axis = plt.subplots()

    facecolor = kwargs.get("facecolor", "k")
    edgecolor = kwargs.get("edgecolor", "none")

    # get the number of tips
    num_tips = len(tree.tips)

    # compute the size of the vertical spacing between branches
    # this should be 5 times the diameter of the first section and there
    # are num_tips + 1 spacing in total.
    init_diam  = tree.root.diameter
    vspace     = 5*init_diam
    tot_height = (num_tips + 1) * vspace

    # compute the total length which is 1.1 times the longest distance
    # to soma
    max_dts    = np.max([n.distance_to_soma() for n in tree.tips])
    tot_length = 1.1*max_dts

    # we need to find the number of up and down children for each node
    queue = deque(tree.root)

    up_children   = {}
    down_children = {}

    while queue:
        node = queue.popleft()
        queue.extend(node.children)

        if node.children:
            up_children[node]   = set(node.children[0])
            down_children[node] = set(node.children[1])

        if node in up_children.get(node.parent, False):
            for val in up_children.values():
                if node.parent is in val:
                    val.add(node)

        if node in down_children.get(node.parent, False):
            for val in down_children.values():
                if node.parent is in val:
                    val.add(node)

    # now we can plot the dendrogram
    x0 = 0.05*max_dts

    parent_y = {}

    queue = deque(tree.root.children[0])

    while queue:
        node = queue.popleft()
        queue.extend(node.children)

        x = x0

        if node.parent is not None:
            x += node.parent.distance_to_soma()

        num_up   = len(up_children[node])
        num_down = len(down_children[node])

        # get parent y
        y = parent_y.get(node.parent, 0.5*vspace)

        if node in up_children.get(node.parent, True):
            y += num_down*vspace - 0.5*node.diameter
        else:
            y -= num_up*vspace + 0.5*node.diameter

        axis.add_artist(
            Rectangle((x, y), node.distance_to_parent, node.diameter,
                      fill=True, facecolor=facecolor,
                      edgecolor=edgecolor))

    axis.set_xlim(0, tot_length)
    axis.set_ylim(0, tot_height)



# ---------------- #
# Plot environment #
# ---------------- #

def plot_environment(culture=None, title='Environment', ax=None, m='',
                    mc="#999999", fc="#ccccff", ec="#444444", alpha=0.5,
                    brightness="height", show=True, **kwargs):
    '''
    Plot the environment in which the neurons grow.

    Parameters
    ----------
    culture : :class:`~dense.environment.Shape`, optional (default: None)
        Shape of the environment; if the environment was already set using
        :func:`~dense.CreateEnvironment`
    title : str, optional (default: 'Shape')
        Title of the plot.
    ax : :class:`matplotlib.axes.Axes`, optional (default: new axis)
        Optional existing axis on which the environment should be added.
    m : str, optional (default: invisible)
        Marker to plot the shape's vertices, matplotlib syntax.
    mc : str, optional (default: "#999999")
        Color of the markers.
    fc : str, optional (default: "#8888ff")
        Color of the shape's interior.
    ec : str, optional (default: "#444444")
        Color of the shape's edges.
    alpha : float, optional (default: 0.5)
        Opacity of the shape's interior.
    brightness : str, optional (default: height)
        Show how different other areas are from the 'default_area' (lower
        values are darker, higher values are lighter).
        Difference can concern the 'height', or any of the `properties` of the
        :class:`Area` objects.
    show : bool, optional (default: False)
        If True, the plot will be displayed immediately.
    '''
    if culture is None:
        culture = _pg.get_environment()
    plot_shape(culture, axis=ax,  m=m, mc=mc, fc=fc, ec=ec, alpha=alpha,
               brightness=brightness, show=show)
