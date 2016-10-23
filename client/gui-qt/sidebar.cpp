/**********************************************************************
 Freeciv - Copyright (C) 1996-2005 - Freeciv Development Team
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
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QTimer>

// qt-client
#include "sidebar.h"
#include "sprite.h"

extern void pixmap_copy(QPixmap *dest, QPixmap *src, int src_x, int src_y,
                        int dest_x, int dest_y, int width, int height);

static void reduce_mod(int &val,  int &mod);

/***************************************************************************
  Helper function to fit tax sprites, reduces modulo, increasing value
***************************************************************************/
void reduce_mod(int &mod,  int &val)
{
  if (mod > 0) {
    val++;
    mod--;
  }

  return;
}

/***************************************************************************
  Sidewidget constructor
***************************************************************************/
fc_sidewidget::fc_sidewidget(QPixmap *pix, QString label, QString pg,
                             pfcn_bool func, int type): QWidget()
{
  if (pix == nullptr) {
    pix = new QPixmap(12,12);
    pix->fill(Qt::black);
  }
  blink = false;
  disabled = false;
  def_pixmap = pix;
  scaled_pixmap = new QPixmap;
  final_pixmap = new QPixmap;
  sfont = new QFont;
  left_click = func;
  desc = label;
  standard = type;
  hover = false;
  right_click = nullptr;
  wheel_down = nullptr;
  wheel_up = nullptr;
  page = pg;
  setContextMenuPolicy(Qt::CustomContextMenu);
  timer = new QTimer;
  timer->setSingleShot(false);
  timer->setInterval(700);
  connect(timer, SIGNAL(timeout()), this, SLOT(sblink()));
}

/***************************************************************************
  Sidewidget destructor
***************************************************************************/
fc_sidewidget::~fc_sidewidget()
{
  if (scaled_pixmap) {
    delete scaled_pixmap;
  }

  if (def_pixmap) {
    delete def_pixmap;
  }
}

/***************************************************************************
  Sets default pixmap for sidewidget
***************************************************************************/
void fc_sidewidget::set_pixmap(QPixmap *pm)
{
  if (def_pixmap) {
    delete def_pixmap;
  }

  def_pixmap = pm;
}

/***************************************************************************
  Sets custom text visible on top of sidewidget
***************************************************************************/
void fc_sidewidget::set_custom_labels(QString l)
{
  custom_label = l;
}

/***************************************************************************
  Sets tooltip for sidewidget
***************************************************************************/
void fc_sidewidget::set_tooltip(QString tooltip)
{
  setToolTip(tooltip);
}

/***************************************************************************
  Reeturns scaled (not default) pixmap for sidewidget
***************************************************************************/
QPixmap *fc_sidewidget::get_pixmap()
{
  return scaled_pixmap;
}

/***************************************************************************
  Sets default label on bottom of sidewidget
***************************************************************************/
void fc_sidewidget::set_label(QString str)
{
  desc = str;
}

/***************************************************************************
  Resizes default_pixmap to scaled_pixmap to fit current width,
  leaves default_pixmap unchanged
***************************************************************************/
void fc_sidewidget::resize_pixmap(int width, int height)
{
  if (standard == SW_TAX) {
    height = get_tax_sprite(tileset, O_LUXURY)->pm->height() + 8;
  }

  if (standard == SW_INDICATORS) {
    height = client_government_sprite()->pm->height() + 8;
  }

  if (def_pixmap) {
    *scaled_pixmap = def_pixmap->scaled(width, height, Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
  }
}

/***************************************************************************
  Paint event for sidewidget
***************************************************************************/
void fc_sidewidget::paintEvent(QPaintEvent *event)
{
  QPainter painter;

  painter.begin(this);
  paint(&painter, event);
  painter.end();
}

/***************************************************************************
  Paints final pixmap on screeen
***************************************************************************/
void fc_sidewidget::paint(QPainter *painter, QPaintEvent *event)
{

  if (final_pixmap) {
    painter->drawPixmap(event->rect(), *final_pixmap,
                        event->rect());
  }
}

/***************************************************************************
  Mouse entered on widget area
***************************************************************************/
void fc_sidewidget::enterEvent(QEvent *event)
{
  if (hover == false) {
    hover = true;
    update_final_pixmap();
    QWidget::enterEvent(event);
    update();
  }
}

/***************************************************************************
  Mouse left widget area
***************************************************************************/
void fc_sidewidget::leaveEvent(QEvent *event)
{
  if (hover) {
    hover = false;
    update_final_pixmap();
    QWidget::leaveEvent(event);
    update();

  }
}

/***************************************************************************
  Context menu requested
***************************************************************************/
void fc_sidewidget::contextMenuEvent(QContextMenuEvent *event)
{
  if (hover) {
    hover = false;
    update_final_pixmap();
    QWidget::contextMenuEvent(event);
    update();
  }
}

/***************************************************************************
  Sets callback for mouse left click
***************************************************************************/
void fc_sidewidget::set_left_click(pfcn_bool func)
{
  left_click = func;
}

/***************************************************************************
  Sets callback for mouse right click
***************************************************************************/
void fc_sidewidget::set_right_click(pfcn func)
{
  right_click = func;
}

/***************************************************************************
  Sets callback for mouse wheel down
***************************************************************************/
void fc_sidewidget::set_wheel_down(pfcn func)
{
  wheel_down = func;
}

/***************************************************************************
  Sets callback for mouse wheel up
***************************************************************************/
void fc_sidewidget::set_wheel_up(pfcn func)
{
  wheel_up = func;
}


/***************************************************************************
  Mouse press event for sidewidget
***************************************************************************/
void fc_sidewidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && left_click != nullptr) {
    left_click(true);
  }
  if (event->button() == Qt::RightButton && right_click != nullptr) {
    right_click();
  }
  if (event->button() == Qt::RightButton && right_click == nullptr) {
    gui()->game_tab_widget->setCurrentIndex(0);
  }
}

/***************************************************************************
  Mouse wheel event
***************************************************************************/
void fc_sidewidget::wheelEvent(QWheelEvent *event)
{
  if (event->delta() < -90 && wheel_down) {
    wheel_down();
  } else if (event->delta() > 90 && wheel_up) {
    wheel_up();
  }

  event->accept();
}


/***************************************************************************
  Blinks current sidebar widget
***************************************************************************/
void fc_sidewidget::sblink()
{
  if (keep_blinking) {
    if (timer->isActive() == false) {
      timer->start();
    }
    blink = !blink;
  } else {
    blink = false;
    if (timer->isActive()) {
      timer->stop();
    }
  }
  update_final_pixmap();
}


/***************************************************************************
  Updates final pixmap and draws it on screen
***************************************************************************/
void fc_sidewidget::update_final_pixmap()
{
  const struct sprite *sprite;
  int w, h, pos, i;
  QPainter p;
  QPen pen;
  bool current = false;

  if (final_pixmap) {
    delete final_pixmap;
  }

  i = gui()->gimme_index_of(page);
  if (i == gui()->game_tab_widget->currentIndex()) {
    current = true;
  }
  final_pixmap = new QPixmap(scaled_pixmap->width(), scaled_pixmap->height());
  final_pixmap->fill(Qt::transparent);

  if (scaled_pixmap->width() == 0 || scaled_pixmap->height() == 0) {
    return;
  }

  p.begin(final_pixmap);
  sfont->setPixelSize(16);
  sfont->setCapitalization(QFont::SmallCaps);
  sfont->setItalic(true);
  p.setFont(*sfont);
  pen.setColor(QColor(232, 255, 0));
  p.setPen(pen);

  if (standard == SW_TAX && client_is_global_observer() == false) {
    pos = 0;
    int d, modulo;
    sprite = get_tax_sprite(tileset, O_GOLD);
    w = width() / 10;
    modulo = width() % 10;
    h = sprite->pm->height();
    reduce_mod(modulo, pos);

    for (d = 0; d < client.conn.playing->economic.tax / 10; ++d) {
      p.drawPixmap(pos, 5, sprite->pm->scaled(w, h), 0, 0, w, h);
      pos = pos + w;
      reduce_mod(modulo, pos);
    }

    sprite = get_tax_sprite(tileset, O_LUXURY);

    for (; d < (client.conn.playing->economic.tax
                + client.conn.playing->economic.luxury) / 10; ++d) {
      p.drawPixmap(pos, 5, sprite->pm->scaled(w, h), 0, 0, w, h);
      pos = pos + w;
      reduce_mod(modulo, pos);
    }

    sprite = get_tax_sprite(tileset, O_SCIENCE);

    for (; d < 10 ; ++d) {
      p.drawPixmap(pos, 5, sprite->pm->scaled(w, h), 0, 0, w, h);
      pos = pos + w;
      reduce_mod(modulo, pos);
    }
  } else if (standard == SW_INDICATORS) {
    sprite = client_research_sprite();
    w = sprite->pm->width();
    pos = scaled_pixmap->width() / 2 - 2 * w;
    p.drawPixmap(pos, 5, *sprite->pm);
    pos = pos + w;
    sprite = client_warming_sprite();
    p.drawPixmap(pos, 5, *sprite->pm);
    pos = pos + w;
    sprite = client_cooling_sprite();
    p.drawPixmap(pos, 5, *sprite->pm);
    pos = pos + w;
    sprite = client_government_sprite();
    p.drawPixmap(pos, 5, *sprite->pm);

  } else {
    p.drawPixmap(0, 0 , *scaled_pixmap);
    p.drawText(0, height() - 6 , desc);
  }

  p.setPen(palette().color(QPalette::Text));

  if (custom_label.isEmpty() == false) {
    sfont->setItalic(false);
    p.setFont(*sfont);
    p.drawText(0, 0, width(), height(), Qt::AlignLeft | Qt::TextWordWrap,
               custom_label);
  }

  if (current) {
    p.setPen(palette().color(QPalette::Highlight));
    p.drawRect(0 , 0, width() - 1 , height() - 1);
  }

  if (hover && !disabled) {
    p.setCompositionMode(QPainter::CompositionMode_ColorDodge);
    p.setPen(palette().color(QPalette::Highlight));
    p.setBrush(palette().color(QPalette::AlternateBase));
    p.drawRect(0 , 0, width() - 1 , height() - 1);
  }

  if (disabled) {
    p.setCompositionMode(QPainter::CompositionMode_Darken);
    p.setPen(QColor(0, 0, 0));
    p.setBrush(QColor(0, 0, 50, 95));
    p.drawRect(0 , 0, width(), height());
  }

  if (blink) {
    p.setCompositionMode(QPainter::CompositionMode_ColorDodge);
    p.setPen(QColor(0, 0, 0));
    p.setBrush(palette().color(QPalette::HighlightedText));
    p.drawRect(0 , 0, width(), height());
  }

  p.end();
  update();
}

/***************************************************************************
  Sidebar constructor
***************************************************************************/
fc_sidebar::fc_sidebar()
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  sidebar_img = nullptr;
  layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);
}

/***************************************************************************
  Sidebar destructor
***************************************************************************/
fc_sidebar::~fc_sidebar()
{

}

/***************************************************************************
  Adds new sidebar widget
***************************************************************************/
void fc_sidebar::add_widget(fc_sidewidget *fsw)
{
  objects.append(fsw);
  layout->addWidget(fsw);
  return;
}

/***************************************************************************
  Paint event for sidebar
***************************************************************************/
void fc_sidebar::paintEvent(QPaintEvent *event)
{
  QPainter painter;

  painter.begin(this);
  paint(&painter, event);
  painter.end();
}


/***************************************************************************
  Paints dark rectangle as background for sidebar
***************************************************************************/
void fc_sidebar::paint(QPainter *painter, QPaintEvent *event)
{
  painter->setBrush(QBrush(QColor(40, 40, 40)));
  painter->drawRect(event->rect());
}

/**************************************************************************
  Resize sidebar to take 100 pixels or 10% of given width, and all
  widgets inside sidebar
**************************************************************************/
void fc_sidebar::resize_me(int wdth, int hght, bool force)
{
  int w, h, non_std, non_std_count;
  w = wdth / 10;
  h = hght;
  w = qMin(100, w);

  if (force == false && w == width() && h == height()) {
    return;
  }

  if (sidebar_img) {
    delete sidebar_img;
  }

  sidebar_img = new QPixmap(w, h);

  non_std = 0;
  non_std_count = 0;

  /* resize all non standard sidewidgets first*/
  foreach (fc_sidewidget * sw,  objects) {
    if (sw->standard != SW_STD) {
      sw->resize_pixmap(w, 0);
      sw->setFixedSize(w, sw->get_pixmap()->height());
      sw->update_final_pixmap();
      non_std = non_std + sw->get_pixmap()->height();
      non_std_count++;
    }
  }

  h = h - non_std;
  h = h / (objects.count() - non_std_count) - 2;
  /* resize all standard sidewidgets */
  foreach (fc_sidewidget * sw,  objects) {
    if (sw->standard == SW_STD) {
      sw->resize_pixmap(w, h);
      sw->setFixedSize(w, h);
      sw->update_final_pixmap();
    }
  }
}


/***************************************************************************
  Callback to show map
***************************************************************************/
void side_show_map(bool nothing)
{
  gui()->game_tab_widget->setCurrentIndex(0);
}

/***************************************************************************
  Callback for finishing turn
***************************************************************************/
void side_finish_turn(bool nothing)
{
  key_end_turn();
}

/***************************************************************************
  Callback to popup rates dialog
***************************************************************************/
void side_rates_wdg(bool nothing)
{
  if (client_is_observer() == false) {
    popup_rates_dialog();
  }
}

/***************************************************************************
  Callback to center on current unit
***************************************************************************/
void side_center_unit()
{
  gui()->game_tab_widget->setCurrentIndex(0);
  request_center_focus_unit();
}

/***************************************************************************
  Disables end turn button if asked
***************************************************************************/
void side_disable_endturn(bool do_restore)
{
  if (gui()->current_page() != PAGE_GAME) {
    return;
  }
  gui()->sw_endturn->disabled = !do_restore;
  gui()->sw_endturn->update_final_pixmap();
}

/***************************************************************************
  Changes background of endturn widget if asked
***************************************************************************/
void side_blink_endturn(bool do_restore)
{
  if (gui()->current_page() != PAGE_GAME) {
    return;
  }
  gui()->sw_endturn->blink = !do_restore;
  gui()->sw_endturn->update_final_pixmap();
}

/***************************************************************************
  Popups menu on indicators widget
***************************************************************************/
void side_indicators_menu()
{
  gov_menu *menu= new gov_menu(gui()->sidebar_wdg);
  menu->create();
  menu->update();
  menu->popup(QCursor::pos());
}

/***************************************************************************
  Shows diplomacy dialog if there is any open
***************************************************************************/
void side_show_diplomacy_dialog(void)
{
  int i;
  i = gui()->gimme_index_of("DDI");
  if (i < 0) {
    return;
  }
  gui()->game_tab_widget->setCurrentIndex(i);
}
