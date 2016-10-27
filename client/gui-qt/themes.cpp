/********************************************************************** 
 Freeciv - Copyright (C) 2005 The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// Qt
#include <QApplication>
#include <QDir>
#include <QStyleFactory>

/* utility */
#include "mem.h"

/* client */
#include "themes_common.h"

// gui-qt
#include "qtg_cxxside.h"

/* client/include */
#include "themes_g.h"

static QString stylestring;
static QString real_data_dir;
extern QApplication *qapp;
extern QString current_theme;
extern QApplication *current_app();
static QString def_app_style;

/*****************************************************************************
  Loads a qt theme directory/theme_name
*****************************************************************************/
void qtg_gui_load_theme(const char *directory, const char *theme_name)
{
  QString name;
  QString res_name;
  QString path;
  QString fake_dir;
  QDir dir;
  QFile f;
  QString lnb = "LittleFinger";
  QPalette pal;

  if (def_app_style.isEmpty()) {
    def_app_style = QApplication::style()->objectName();
  }

  if (real_data_dir.isEmpty()) {
    real_data_dir = QString(directory);
  }
  
  path = real_data_dir + DIR_SEPARATOR + theme_name + DIR_SEPARATOR;
  name = dir.absolutePath() + QDir::separator() + real_data_dir;
  name = path + "resource.qss";
  f.setFileName(name);

  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (QString(theme_name) != QString("NightStalker")) {
      qtg_gui_clear_theme();
    }
    return;
  }
  /* Stylesheet uses UNIX separators */
  fake_dir = real_data_dir;
  fake_dir.replace(QString(DIR_SEPARATOR), "/");
  QTextStream in(&f);
  stylestring = in.readAll();
  stylestring.replace(lnb, fake_dir + "/" + theme_name + "/");

  if (QString(theme_name) == QString("System")) {
    QApplication::setStyle(QStyleFactory::create(def_app_style));
  } else {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
  }

  current_theme = theme_name;
  current_app()->setStyleSheet(stylestring);
  if (gui()) {
    gui()->reload_sidebar_icons();
  }
  pal.setBrush(QPalette::Link, QColor(92,170,229));
  pal.setBrush(QPalette::LinkVisited, QColor(54,150,229));
  QApplication::setPalette(pal);
}

/*****************************************************************************
  Clears a theme (sets default system theme)
*****************************************************************************/
void qtg_gui_clear_theme()
{
  QString name, str;

  str = QString("themes") + DIR_SEPARATOR + "gui-qt" + DIR_SEPARATOR;
  name = fileinfoname(get_data_dirs(), str.toLocal8Bit().data());
  qtg_gui_load_theme(name.toLocal8Bit().data(), "NightStalker");
}

/*****************************************************************************
  Each gui has its own themes directories.

  Returns an array containing these strings and sets array size in count.
  The caller is responsible for freeing the array and the paths.
*****************************************************************************/
char **qtg_get_gui_specific_themes_directories(int *count)
{
  char **array;
  char *persistent = static_cast<char*>(fc_malloc(256));

  *count = 1;
  array = new char *[*count];
  strncpy(persistent, fileinfoname(get_data_dirs(),""), 256);
  array[0] = persistent;
  return array;
}

/*****************************************************************************
  Return an array of names of usable themes in the given directory.
  Array size is stored in count.
  The caller is responsible for freeing the array and the names
*****************************************************************************/
char **qtg_get_useable_themes_in_directory(const char *directory, int *count)
{
  QStringList sl, theme_list;
  char **array;
  char *data;
  QByteArray qba;;
  QString str;
  QString name;
  QDir dir;
  QFile f;

  str = QString("themes") + DIR_SEPARATOR + "gui-qt" + DIR_SEPARATOR;
  name = fileinfoname(get_data_dirs(), str.toLocal8Bit().data());

  dir.setPath(name);
  sl << dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
  name = name;

  foreach(str, sl) {
    f.setFileName(name + str + DIR_SEPARATOR + "resource.qss");
    if (f.exists() == false) {
      continue;
    }
    theme_list << str;
  }
  
  array = new char *[theme_list.count()];
  *count = theme_list.count();

  for (int i = 0; i < *count; i++) {
    qba = theme_list[i].toLocal8Bit();
    data = new char[theme_list[i].toLocal8Bit().count() + 1];
    strcpy(data, theme_list[i].toLocal8Bit().data());
    array[i] = data;
  }
  
  return array;
}
