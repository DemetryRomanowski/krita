import pykrita
import os
import sys

from .api import *
from .decorators import *
from .dockwidgetfactory import *
from PyKrita.krita import *

def qDebug(text):
    '''Use KDE way to show debug info

        TODO Add a way to control debug output from partucular plugins (?)
    '''
    plugin = sys._getframe(1).f_globals['__name__']
    pykrita.qDebug('{}: {}'.format(plugin, text))


@pykritaEventHandler('_pluginLoaded')
def on_load(plugin):
    if plugin in init.functions:
        # Call registered init functions for the plugin
        init.fire(plugin=plugin)
        del init.functions[plugin]
    return True


@pykritaEventHandler('_pluginUnloading')
def on_unload(plugin):
    if plugin in unload.functions:
        # Deinitialize plugin
        unload.fire(plugin=plugin)
        del unload.functions[plugin]
    return True


@pykritaEventHandler('_pykritaLoaded')
def on_pykrita_loaded():
    qDebug('PYKRITA LOADED')
    return True


@pykritaEventHandler('_pykritaUnloading')
def on_pykrita_unloading():
    qDebug('UNLOADING PYKRITA')
    return True



sys._excepthook = sys.excepthook
def exception_hook(exctype, value, traceback):
    print("Definining exception hook")
    sys._excepthook(exctype, value, traceback)
    #sys.exit(1)
sys.excepthook = exception_hook
