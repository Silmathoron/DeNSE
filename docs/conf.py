# -*- coding: utf-8 -*-
#
# conf.py
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

"""
DeNSE documentation build configuration file.
"""

import fnmatch
import os
import sys
from subprocess import call
import sphinx_bootstrap_theme

doc_path = os.path.abspath(".")
sys.path.insert(0, doc_path)

from conf_helpers import gen_autosum, pygrowth_mocker, configure_doxygen


# -- Set doxygen/breathe --------------------------------------------------

root_path = os.path.abspath("..")
doxypath = doc_path + '/doxyfiles/xml'
sys.path.append(doc_path + '/breathe')
sys.path.append(doxypath)
sys.path.insert(0, doc_path)
sys.path.insert(0, root_path + "/src/pymodule")
sys.path.insert(0, root_path)

# run doxygen
configure_doxygen()
call(["doxygen", "Doxyfile"])


# General information about the project.
project = u'DeNSE'
copyright = u'2019, SENeC Initiative'
author = u'SENeC Initiative'


# -- Mock _pygrowth file -----------------------------------------------------

pygrowth_mocker()

import pg_mock

sys.modules["dense._pygrowth"] = pg_mock

import dense


# -- Setup all autosum then start --------------------------------------------

# find all *.in files

inputs = []
for root, dirnames, filenames in os.walk(root_path):
    for filename in fnmatch.filter(filenames, '*.rst.in'):
        inputs.append(os.path.join(root, filename))

for f in inputs:
    target = f[:-3]  # remove '.in'
    # find the module (what will come after dense, it is the name of the file)
    last_dot = target.rfind('.')
    last_slash = target.rfind('/')
    module = target[last_slash + 1:last_dot]
    if module != 'dense':
        module = 'dense.' + module
    gen_autosum(f, target, module, 'summary')


# -- General configuration ------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
#
# needs_sphinx = '1.0'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'sphinx.ext.coverage',
    'sphinx.ext.mathjax',
    'sphinx.ext.viewcode',
    'sphinx.ext.autosummary',
    'sphinx.ext.autodoc',
    'sphinx.ext.napoleon',
    'breathe'
]

breathe_projects = {project: doxypath}
breathe_default_project = project
breathe_projects_source = {project: root_path}

# Add any paths that contain templates here, relative to this directory.
templates_path = [doc_path + '/_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
#
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

# The master toctree document.
master_doc = 'index'

rst_epilog = """
.. |name| replace:: DeNSE
"""

# The version info for the project you're documenting, acts as replacement for
# |version| and |release|, also used in various other places throughout the
# built documents.
#
# The short X.Y version.
version = dense.__version__
# The full version, including alpha/beta/rc tags.
release = version

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
#
# This is also used if you do content translation via gettext catalogs.
# Usually you set "language" from the command line for these cases.
language = None

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This patterns also effect to html_static_path and html_extra_path
exclude_patterns = []

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# If true, `todo` and `todoList` produce output, else they produce nothing.
todo_include_todos = True


# -- Options for HTML output ----------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'dense_theme'
html_use_smartypants = False

html_theme_path = [doc_path] + \
                    sphinx_bootstrap_theme.get_html_theme_path()

html_theme_options = {
    'navbar_links': [
        ("Modules", "py-modindex"),
        ("Index", "genindex"),
        #("GitHub", "https://github.com/Silmathoron/NNGT", True),
    ],

    # Render the next and previous page links in navbar. (Default: true)
    'navbar_sidebarrel': False,

    # Render the current pages TOC in the navbar. (Default: true)
    'navbar_pagenav': True,

    # Tab name for the current pages TOC. (Default: "Page")
    'navbar_pagenav_name': "Current",

    # Global TOC depth for "site" navbar tab. (Default: 1)
    # Switching to -1 shows all levels.
    'globaltoc_depth': 2,

    # Include hidden TOCs in Site navbar?
    #
    # Note: If this is "false", you cannot have mixed ``:hidden:`` and
    # non-hidden ``toctree`` directives in the same page, or else the build
    # will break.
    #
    # Values: "true" (default) or "false"
    'globaltoc_includehidden': "true",

    # Fix navigation bar to top of page?
    # Values: "true" (default) or "false"
    'navbar_fixed_top': "false",

    # Location of link to source.
    # Options are "nav" (default), "footer" or anything else to exclude.
    'source_link_position': "",

    # Bootswatch (http://bootswatch.com/) theme.
    'bootswatch_theme': "cosmo"
}

html_sidebars = {'**': ['customtoc.html', 'searchbox.html']}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = [doc_path + '/_static']


# -- Options for HTMLHelp output ------------------------------------------

# Output file base name for HTML help builder.
htmlhelp_basename = 'DeNSE_doc'


# -- Options for LaTeX output ---------------------------------------------

latex_elements = {
    # The paper size ('letterpaper' or 'a4paper').
    #
    # 'papersize': 'letterpaper',

    # The font size ('10pt', '11pt' or '12pt').
    #
    # 'pointsize': '10pt',

    # Additional stuff for the LaTeX preamble.
    #
    # 'preamble': '',

    # Latex figure (float) alignment
    #
    # 'figure_align': 'htbp',
}

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title,
#  author, documentclass [howto, manual, or own class]).
latex_documents = [
    (master_doc, 'DeNSE.tex', u'DeNSE Documentation',
     author, 'manual'),
]


# -- Options for manual page output ---------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    (master_doc, 'DeNSE', u'DeNSE Documentation',
     [author], 1)
]


# -- Options for Texinfo output -------------------------------------------

# Grouping the document tree into Texinfo files. List of tuples
# (source start file, target name, title, author,
#  dir menu entry, description, category)
texinfo_documents = [
    (master_doc, 'DeNSE', u'DeNSE Documentation',
     author, 'DeNSE', 'One line description of project.',
     'Miscellaneous'),
]




# Example configuration for intersphinx: refer to the Python standard library.
intersphinx_mapping = {'https://docs.python.org/': None}

napoleon_numpy_docstring = True
napoleon_use_admonition_for_notes = True
