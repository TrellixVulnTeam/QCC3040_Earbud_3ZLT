############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""Dynamic Live Graph Module.

This module provides a ``LiveGraph`` class which takes a JSON configuration
file and python object and creates a live updating matplotlib plot (via
``funcanimation``) of the specified attributes of the python object polled at a
set interval.

"""
from collections import deque
import os
from tkinter import filedialog
import warnings

import matplotlib
from matplotlib import animation
from matplotlib import patches as pch
from matplotlib.backend_tools import ToolBase

from aanclogger.connect import CONN_LEFT, CONN_RIGHT
from aanclogger.schema import VALIDATOR

# Force a backend to try and improve compatibility. This has to come before
# pyplot is imported so there are pylint warnings about import position below.
matplotlib.use('TkAgg', force=True)
from matplotlib import pyplot as plt # pylint: disable=wrong-import-position, ungrouped-imports, wrong-import-order

# Configuration values for launching the plotting tool
plt.rcParams['toolbar'] = 'toolmanager'
RDIR = os.path.join(os.path.dirname(__file__), 'resources')
PAUSE_ICON = os.path.join(RDIR, 'pause_icon.gif')
PLAY_ICON = os.path.join(RDIR, 'play_icon.gif')
SAVE_ICON = os.path.join(RDIR, 'save_icon.gif')

# Define the graph schema
OPERATOR_KEY = 'operators'
OPERATOR_NAME = 'name'
OPERATOR_CAP_ID = 'cap_id'
OPERATOR_CAP_IDX = 'cap_idx'

OPERATOR_CONFIG = {
    "type": "object",
    "properties": {
        OPERATOR_NAME: {"type": "string"},
        OPERATOR_CAP_ID: {"type": "number"},
        OPERATOR_CAP_IDX: {"type": "number"}
    },
    "required": [
        OPERATOR_NAME,
        OPERATOR_CAP_ID,
        OPERATOR_CAP_IDX
    ]
}

HANDLES_KEY = 'handles'
HANDLE_NAME = 'name'
HANDLE_OPERATOR = 'operator'
HANDLE_ATTR = 'attr'
HANDLE_CONVERSION = 'conversion'
CONVERSION_SIGNED = 'signed'
CONVERSION_QFMT = 'qfmt'
CONVERSION_BITMASK = 'bitmask'
CONVERSION_SCALE = 'scale'
CONVERSION_OFFSET = 'offset'
HANDLE_LOG_FMT = 'log_fmt'
HANDLE_MISSING = 'missing'

DEFAULT_CONVERSION_SIGNED = False
DEFAULT_CONVERSION_QFMT = ""
DEFAULT_CONVERSION_BITMASK = ""
DEFAULT_CONVERSION_SCALE = 1
DEFAULT_CONVERSION_OFFSET = 0
DEFAULT_HANDLE_LOG_FMT = "%08X"
DEFAULT_HANDLE_MISSING = -1

CONVERSION_CONFIG = {
    "type": "object",
    "properties": {
        CONVERSION_SIGNED: {
            "type": "boolean",
            "default": DEFAULT_CONVERSION_SIGNED
            },
        CONVERSION_QFMT: {
            "type": "string",
            "default": DEFAULT_CONVERSION_QFMT
            },
        CONVERSION_BITMASK: {
            "type": "string",
            "default": DEFAULT_CONVERSION_BITMASK
            },
        CONVERSION_SCALE: {
            "oneOf": [
                {"type": "number"},
                {"type": "string"}
            ],
            "default": DEFAULT_CONVERSION_SCALE
        },
        CONVERSION_OFFSET: {
            "type": "number",
            "default": DEFAULT_CONVERSION_OFFSET
        }
    }
}

HANDLES_CONFIG = {
    "type": "object",
    "properties": {
        HANDLE_NAME: {"type": "string"},
        HANDLE_OPERATOR: {"type": "string"},
        HANDLE_ATTR: {
            "oneOf": [
                {"type": "string"},
                {"type": "number"}
            ],
        },
        HANDLE_CONVERSION: CONVERSION_CONFIG,
        HANDLE_LOG_FMT: {"type": "string", "default": DEFAULT_HANDLE_LOG_FMT},
        HANDLE_MISSING: {"type": "number", "default": DEFAULT_HANDLE_MISSING}
    },
    "required": [
        HANDLE_NAME,
        HANDLE_OPERATOR,
        HANDLE_ATTR,
        HANDLE_CONVERSION
    ]
}

PLOT_PROPERTIES = 'properties'
PLOT_SIZE = 'size'
PLOT_NPLT = 'nplt'
PLOT_NSAVE = 'nsave'
PLOT_REFRESH = 'refresh_rate'
PLOT_TITLE = 'title'
PLOT_X = 'fig_x'
PLOT_Y = 'fig_y'

PLOT_DEFAULTS = 'defaults'
PLOT_GRID = 'grid'
PLOT_LEGEND = 'legend_location'
PLOT_MARKER = 'marker'

PLOT_GRAPHS = 'graphs'
PLOT_SUBPLOT = 'subplot'
PLOT_YLIM = 'ylim'
PLOT_YLABEL = 'ylabel'
PLOT_LINES = 'lines'
PLOT_ATTR = 'attr'
PLOT_LABEL = 'label'
PLOT_COLOR = 'color'
PLOT_FORMAT = 'format'

PLOT_SCHEMA = {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        OPERATOR_KEY: {
            "type": "object",
            "properties": {
                CONN_LEFT: {
                    "type": "array",
                    "default": [],
                    "items": OPERATOR_CONFIG
                },
                CONN_RIGHT: {
                    "type": "array",
                    "default": [],
                    "items": OPERATOR_CONFIG
                }
            }
        },
        HANDLES_KEY: {
            "type": "array",
            "default": [],
            "items": HANDLES_CONFIG
        },
        PLOT_PROPERTIES : {
            "type": "object",
            "properties": {
                PLOT_SIZE: {
                    "type": "array",
                    "default": [7.5, 9],
                    "items": {
                        "type": "number"
                    },
                    "minItems": 2,
                    "maxItems": 2
                },
                PLOT_NPLT: {"type": "integer", "default": 250},
                PLOT_NSAVE: {"type": "integer", "default": 1500},
                PLOT_REFRESH: {"type": "number", "default": 25},
                PLOT_TITLE: {"type": "string", "default": "AANC"},
                PLOT_X: {"type": "integer", "default": 500},
                PLOT_Y: {"type": "integer", "default": 50}
            }
        },
        PLOT_DEFAULTS: {
            "type": "object",
            "properties": {
                PLOT_GRID: {"type": "string", "default": "both"},
                PLOT_LEGEND: {"type": "string", "default": "upper left"},
                PLOT_MARKER: {"type": "string", "default": ""}
            }
        },
        PLOT_GRAPHS: {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    PLOT_TITLE: {"type": "string"},
                    PLOT_SUBPLOT: {
                        "type": "array",
                        "default": [],
                        "items": {
                            "type": "number"
                        },
                        "minItems": 3,
                        "maxItems": 3
                    },
                    PLOT_YLIM: {
                        "type": "array",
                        "default": [],
                        "items": {
                            "type": "number"
                        },
                        "minItems": 2,
                        "maxItems": 2
                    },
                    PLOT_YLABEL: {"type": "string"},
                    PLOT_LINES: {
                        "type": "array",
                        "default": [],
                        "items": {
                            "type": "object",
                            "properties": {
                                PLOT_ATTR: {"type": "string"},
                                PLOT_LABEL: {"type": "string"},
                                PLOT_COLOR: {"type": "string",
                                             "default": "#CC0000"},
                                PLOT_MARKER: {"type": "string", "default": ""}
                            }
                        }
                    }
                }
            }
        }
    },
    "required": [
        OPERATOR_KEY,
        HANDLES_KEY
    ]
}

class NoOperatorsException(Exception):
    """Represent trying to read when there are no operators in the graph."""

class LineAttribute(object): # pylint: disable=too-few-public-methods
    """Represent a line plotted on the graph.

    Args:
        name (str): Attribute that the line is associated with
        color (str): Matplotlib-style color for the line
        marker (str): Matplotlib-style marker for the line
        label (str): Label for the line

    """
    def __init__(self, name, color, marker, label): # pylint: disable=too-many-arguments
        """__init__ method."""
        self.name = name
        """str: Name of the attribute that the line is associated with."""
        self.color = color
        """str: Matplotlib-style color for the line."""
        self.marker = marker
        """str: Matplotlib-style marker for the line."""
        self.label = label
        """str: Label for the line."""
        self.operation = None

        schar = None
        if '-' in self.name:
            self.operation = '__sub__'
            schar = '-'
        elif '+' in self.name:
            self.operation = '__add__'
            schar = '+'
        elif '*' in self.name:
            self.operation = '__mul__'
            schar = '*'
        elif '/' in self.name:
            self.operation = '__truediv__'
            schar = '/'

        if self.operation is not None:
            self.name = [name.strip() for name in self.name.split(schar)]


class PlotAttribute(object):
    """Represent an attribute to be plotted.

    This class contains all of the necessary information to get the data for and
    plot an attribute.

    Args:
        handle (object): Handle to read the attribute.
        attr (LineAttribute): Line attributes.
        nplt (int): number of values to plot for the attribute.
        nsave (int): number of values to save for the attribute.

    """
    def __init__(self, handle, attr, nplt, nsave): # pylint: disable=too-many-arguments
        """__init__ method."""
        self.handle = handle
        """object: Handle to read the attribute."""
        self.attr = attr
        """LineAttribute: Line attributes."""
        self.nsave = nsave
        """int: number of values to save for the attribute."""
        self.nplt = nplt
        """int: number of values to plot for the attribute."""
        self.data = [None] * nplt
        """list(float): List of values to plot."""
        self.queue = deque(maxlen=nsave)
        """queue: Queue of values to save."""

    @property
    def value(self):
        """float: value pointed to by this attribute."""
        if isinstance(self.attr.name, list):
            value1 = self.handle.read(self.attr.name[0])
            value2 = self.handle.read(self.attr.name[1])
            return value1.__getattribute__(self.attr.operation)(value2)

        return self.handle.read(self.attr.name)

    def update(self):
        """Update the internal data list and queue."""
        value = self.value
        self.data[:-1] = self.data[1:]
        self.data[-1] = value

        self.queue.append(value)

    def dump(self):
        """Dump the internal queue for saving data.

        Returns:
            list(float): List of values from the internal queue.
        """
        return [self.queue.popleft() for _ in range(len(self.queue))]

    def __str__(self):
        """str: representation of the object."""
        return '%s = %s (%s)' % (self.attr.label, str(self.value),
                                 self.attr.color)

    def __repr__(self):
        """str: representation of the object."""
        return str(self)


def validate_coords(event, xcoord, ycoord):
    """Update co-ordinates if the event data is valid.

    Args:
        event (matplotlib.backend_bases.MouseEvent): event data from mouse
            click/movement.
        xcoord (int): last valid x co-ordinate.
        ycoord (int): last valid y co-ordinate.

    Returns:
        tuple(int, int): Tuple of validated co-ordinates.
    """
    xct, yct = event.xdata, event.ydata

    xco = xct if xct is not None else xcoord
    yco = yct if yct is not None else ycoord

    return (xco, yco)


class LiveGraph(object): # pylint: disable=too-many-instance-attributes
    """Dynamic graph class.

    This parses a python dictionary to construct a dynamic graph of different
    attributes from the input object.

    Args:
        definition (dict): Definition to parse.
        obj (object): Object to query attributes from.

    """
    def __init__(self, definition, obj):
        """__init__ method."""
        self._def = definition
        self._obj = obj

        VALIDATOR(PLOT_SCHEMA).validate(definition)

        self.graphs = self._def[PLOT_GRAPHS]
        """dict: Graphs to plot."""
        self.properties = self._def[PLOT_PROPERTIES]
        """dict: Properties for the plot."""
        self.defaults = self._def[PLOT_DEFAULTS]
        """dict: Defaults for the plot."""

        self._handles = [
            [self.create_line(line) for line in plot[PLOT_LINES]]
            for plot in self.graphs]

        self._fig = None
        self._axs = [None for _ in self.graphs]
        self._lines = [None for _ in self.graphs]
        self._leg = [None for _ in self.graphs]
        self._leglines = dict()

        self._init_axs = [None for _ in self.graphs]
        self._xc0, self._yc0, self._xc1, self._yc1 = (0, 0, 0, 0)
        self._xcu, self._ycu = (0, 0)
        self._sel_ax = None
        self._zoomrect = None

        self._pause = False
        self._ani = None
        self._valid = False

        self.reload()

    def reload(self):
        """Reload operators & handles."""
        try:
            self._obj.reload(self._def[OPERATOR_KEY], self._def[HANDLES_KEY])
        except NoOperatorsException as noe:
            print("Refresh failed: %s" % noe.args[0])

    def create_line(self, line):
        """Convert a line dictionary into the required PlotAttribute.

        This includes parsing the offset and picking up any default properties.

        Args:
            line (dict): Dictionary definition for the line.

        Returns:
            PlotAttribute: Converted representation as a PlotAttribute.
        """
        handle = self._obj
        attr = line['attr']
        marker = self.get_graph_attribute(line, 'marker', '')

        attr = LineAttribute(attr, line['color'], marker, line['label'])

        return PlotAttribute(handle, attr, self.properties[PLOT_NPLT],
                             self.properties[PLOT_NSAVE])

    def get_graph_attribute(self, graph, attr, default):
        """Find an attribute in the input graph or return the default value.

        Args:
            graph (dict): dictionary describing the graph or line.
            attr (str): dictionary key to search.
            default (any): default value.

        Returns:
            Attribute value or default.
        """
        value = default
        if self.defaults and attr in self.defaults.keys():
            value = self.defaults[attr]

        if attr in graph.keys():
            value = graph[attr]

        return value

    def onclickdown(self, event):
        """Callback when mouse is pressed.

        This is used to create the zoom behavior on the graphs:
            * Double click: returns to the initial zoom.
            * Single click: start drawing the zoom box.

        Args:
            event (matplotlib.backend_bases.MouseEvent): Event data.
        """
        for idx, axs in enumerate(self._axs):
            if event.inaxes == axs:
                if event.dblclick:
                    axs.axis(self._init_axs[idx])
                else:
                    self._sel_ax = axs
                    self._xc0, self._yc0 = event.xdata, event.ydata
                    self._zoomrect = pch.Rectangle(
                        (self._xc0, self._yc0), 0, 0, fill=False, linestyle=':')
                    axs.add_patch(self._zoomrect)
                if self._pause:
                    self._fig.canvas.draw_idle()

    def onclickrelease(self, event):
        """Callback when mouse is released.

        This is used to create the zoom behavior on the graphs:
            * Left click release while zooming will zoom the axes
            * Right click release zooms out by 2

        Args:
            event (matplotlib.backend_bases.MouseEvent): Event data.
        """
        (self._xc1, self._yc1) = validate_coords(event, self._xc1, self._yc1)

        if (event.button == 1) & (self._zoomrect is not None):
            # Left click up zooms in to the rectangle if it exists
            if (self._xc0 != self._xc1) & (self._yc0 != self._yc1):
                (xc0, xc1) = sorted([self._xc0, self._xc1])
                (yc0, yc1) = sorted([self._yc0, self._yc1])
                self._sel_ax.axis([xc0, xc1, yc0, yc1])
                if self._pause:
                    self._fig.canvas.draw_idle()
        elif event.button == 3 & (self._sel_ax is not None):
            # Right click zooms out
            axv = self._sel_ax.axis()
            xmid = (axv[0] + axv[1]) / 2.0
            ymid = (axv[2] + axv[3]) / 2.0
            xcr = axv[1] - axv[0]
            ycr = axv[3] - axv[2]
            axnew = [xmid - xcr, xmid + xcr, ymid - ycr, ymid + ycr]
            self._sel_ax.axis(axnew)
            if self._pause:
                self._fig.canvas.draw_idle()

        if self._zoomrect:
            self._zoomrect.remove()
            if self._pause:
                self._fig.canvas.draw_idle()
        self._zoomrect = None
        self._sel_ax = None

    def drawrect(self, event):
        """Callback to draw rectangle when mouse is moved after being pressed.

        Args:
            event (matplotlib.backend_bases.MouseEvent): Event data.
        """
        if self._zoomrect:
            (self._xcu, self._ycu) = validate_coords(
                event, self._xcu, self._ycu)
            self._zoomrect.set_width(self._xcu - self._xc0)
            self._zoomrect.set_height(self._ycu - self._yc0)
            if self._pause:
                self._fig.canvas.draw_idle()

    def onpick(self, event):
        """Callback when legend is selected.

        This allows line visibility to be controlled from the legend box.

        Args:
            event (matplotlib.backend_bases.MouseEvent): Event data.
        """
        legline = event.artist
        origline = self._leglines[legline]
        vis = not origline.get_visible()
        origline.set_visible(vis)
        if vis:
            legline.set_alpha(1.0)
        else:
            legline.set_alpha(0.2)
        if self._pause:
            self._fig.canvas.draw_idle()

    def update(self, _):
        """Update the plot."""
        if not self._pause:
            for graph, handle in zip(self._lines, self._handles):
                for line, attr in zip(graph, handle):
                    line.set_ydata(attr.data)

    def refresh(self):
        """Refresh data for plotting."""
        if self._pause:
            yield True

        self._valid = self._obj.valid

        if not self._valid:
            self.pause()
            yield True

        for handle in self._handles:
            for line in handle:
                line.update()

        yield True

    def pause(self, _=None):
        """Pause plot updates."""
        self._pause = True
        self._ani.event_source.stop()

    def play(self, _=None):
        """Resume plot updates only if data is valid."""
        if not self._valid:
            self.reload()
            self._valid = self._obj.valid

        if not self._valid:
            return

        self._pause = False
        self._ani.event_source.start()

    def save(self, _=None):
        """Save plot data."""
        pre_paused = self._pause

        self.pause()

        headers = []
        data = []
        for handle in self._handles:
            headers += [line.attr.label for line in handle]
            for line in handle:
                if isinstance(line.attr.name, list):
                    fmt = self._obj.log_fmt(line.attr.name[0])
                else:
                    fmt = self._obj.log_fmt(line.attr.name)
                subdata = [fmt % elem for elem in line.dump()]
                data.append(subdata)

        headers = ','.join(headers)
        datatxt = []
        for line in zip(*data):
            datatxt += [','.join(line)]
        datatxt = '\n'.join(datatxt)

        try:
            with filedialog.asksaveasfile(
                    mode='w',
                    filetypes=(("csv files", "*.csv"), ("all files", "*.*")),
                    defaultextension='.csv'
                ) as fid:

                if fid is None:
                    self.play()
                    return
                fid.write(headers)
                fid.write('\n')
                fid.write(datatxt)
        except AttributeError:
            pass

        if not pre_paused:
            self.play()

    def connect_events(self):
        """Connect mouse events and add toolbar buttons."""
        self._fig.canvas.mpl_connect('pick_event', self.onpick)
        self._fig.canvas.mpl_connect('button_press_event', self.onclickdown)
        self._fig.canvas.mpl_connect('button_release_event',
                                     self.onclickrelease)
        self._fig.canvas.mpl_connect('motion_notify_event', self.drawrect)

        with warnings.catch_warnings():
            warnings.simplefilter("ignore")

            self._fig.canvas.manager.toolmanager.add_tool('pause', PauseTool)
            self._fig.canvas.manager.toolbar.add_tool('pause', 'livegraph')
            self._fig.canvas.manager.toolmanager.toolmanager_connect(
                'tool_trigger_pause', self.pause)

            self._fig.canvas.manager.toolmanager.add_tool('play', PlayTool)
            self._fig.canvas.manager.toolbar.add_tool('play', 'livegraph')
            self._fig.canvas.manager.toolmanager.toolmanager_connect(
                'tool_trigger_play', self.play)

            self._fig.canvas.manager.toolmanager.add_tool('savegraph', SaveTool)
            self._fig.canvas.manager.toolbar.add_tool('savegraph', 'livegraph')
            self._fig.canvas.manager.toolmanager.toolmanager_connect(
                'tool_trigger_savegraph', self.save)

    def update_legend(self, graph, cax, lines):
        """Update the legend entries when drawing axes.

        Args:
            graph (dict): Graph properties.
            cax (matplotlib.axes): Axis associated with the legend.
            lines (list(matplotlib.lines)): Lines associated with the legend.

        Returns:
            matplotlib.legend: Updated legend.
        """
        leg_loc = self.get_graph_attribute(graph, 'legend_location',
                                           'upper right')
        leg = cax.legend(loc=leg_loc)
        leglines = dict()
        for legline, line in zip(leg.get_lines(), lines):
            legline.set_picker(True)
            legline.set_pickradius(5)
            leglines[legline] = line

        self._leglines.update(leglines)
        return leg

    def plot(self):
        """Plot the graph."""
        figsize = self.properties['size']

        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            self._fig = plt.figure(num=self.properties[PLOT_TITLE],
                                   figsize=figsize)

        xax = range(self.properties[PLOT_NPLT])

        for idx, (graph, handles) in enumerate(zip(self.graphs, self._handles)):
            cax = self._fig.add_subplot(*graph['subplot'])
            cax.set_title(graph['title'])
            cax.set_xlabel('Sample')
            if 'ylabel' in graph.keys():
                cax.set_ylabel(graph['ylabel'])

            cax.grid(which=self.get_graph_attribute(graph, 'grid', 'both'))

            if 'ylim' in graph.keys():
                cax.set_ylim(graph['ylim'])

            cax.set_xlim([0, self.properties[PLOT_NPLT]])

            lines = [None for _ in handles]
            for cidx, handle in enumerate(handles):
                # handle.update()
                lines[cidx], = cax.plot(xax, handle.data,
                                        color=handle.attr.color,
                                        label=handle.attr.label,
                                        marker=handle.attr.marker)

            self._init_axs[idx] = cax.axis()
            self._lines[idx] = lines
            self._axs[idx] = cax
            self._leg[idx] = self.update_legend(graph, cax, lines)

        plt.tight_layout()
        self.connect_events()

        self._ani = animation.FuncAnimation(
            self._fig, self.update, self.refresh,
            interval=self.properties[PLOT_REFRESH], blit=False, repeat=True)

        coord = (self.properties[PLOT_X], self.properties[PLOT_Y])
        self._fig.canvas.manager.window.wm_geometry('+%d+%d' % coord)

        plt.show()


class PauseTool(ToolBase):
    """Toolbar button to pause the capture."""
    description = 'Pause the capture'
    default_keymap = 'H'
    image = PAUSE_ICON


class PlayTool(ToolBase):
    """Toolbar button to resume the capture."""
    description = 'Resume the capture'
    default_keymap = 'R'
    image = PLAY_ICON


class SaveTool(ToolBase):
    """Toolbar button to save the capture data."""
    description = 'Save the data'
    default_keymap = 'S'
    image = SAVE_ICON
