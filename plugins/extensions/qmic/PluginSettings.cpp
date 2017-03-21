/*
 *  Copyright (c) 2017 Boudewijn Rempt <boud@kogmbh.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "PluginSettings.h"

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>

#include <kconfiggroup.h>
#include <klocalizedstring.h>

#include <KoIcon.h>

#include "kis_config.h"

PluginSettings::PluginSettings(QWidget *parent)
    : KisPreferenceSet(parent)
{
}

PluginSettings::~PluginSettings()
{
}

QString PluginSettings::id()
{
    return QString("qmicsettings");
}

QString PluginSettings::name()
{
    return header();
}

QString PluginSettings::header()
{
    return QString(i18n("G'Mic-Qt Integration"));
}


QIcon PluginSettings::icon()
{
    return koIcon("gmic"); // XXX: Replace with Animtim's G'Mic icon
}


void PluginSettings::savePreferences() const
{
    Q_EMIT(settingsChanged());
}

void PluginSettings::loadPreferences()
{
}

void PluginSettings::loadDefaultPreferences()
{
}
