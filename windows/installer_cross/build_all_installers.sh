#!/bin/bash
#/**********************************************************************
# Freeciv - Copyright (C) 2017
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#***********************************************************************/

if test "x$1" = x || test "x$1" = "x-h" || test "x$1" = "x--help" ; then
  echo "Usage: $0 <crosser dir>"
  exit 1
fi

DLLSPATH="$1"

if ! test -d "$DLLSPATH" ; then
  echo "Dllstack directory \"$DLLSPATH\" not found!" >&2
  exit 1
fi

if ! test -f "$DLLSPATH/crosser.txt" ; then
  echo "Directory \"$DLLSPATH\" does not look like crosser environment!" >&2
  exit 1
fi

RET=0

if ! ./installer_build.sh $DLLSPATH gtk3.22 ; then
  RET=1
  GTK322="Fail"
else
  GTK322="Success"
fi

if ! ./installer_build.sh $DLLSPATH qt ; then
  RET=1
  QT="Fail"
else
  QT="Success"
fi

if ! ./installer_build.sh $DLLSPATH sdl2 ; then
  RET=1
  SDL2="Fail"
else
  SDL2="Success"
fi

if ! ./installer_build.sh $DLLSPATH ruledit ; then
  RET=1
  RULEDIT="Fail"
else
  RULEDIT="Success"
fi

echo "Gtk3.22: $GTK322"
echo "Qt:      $QT"
echo "Sdl2:    $SDL2"
echo "Ruledit: $RULEDIT"

exit $RET
