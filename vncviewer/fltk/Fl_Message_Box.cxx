/* Copyright 2022-2024 Pierre Ossman for Cendio AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>

#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
#include <FL/fl_ask.H>

#include "Fl_Message_Box.h"
#include "layout.h"

static void button_cb(Fl_Widget *w, long) {
  w->window()->hide();
}

Fl_Message_Box_::Fl_Message_Box_(const char* title, const char* icon)
  : Fl_Window(0, 0, title)
{
  int x, y;

  Fl_Box* box;
  Fl_Button* button;

  x = OUTER_MARGIN;
  y = OUTER_MARGIN;

  // Icon
  box = new Fl_Box(x, y, 50, 50, icon);
  box->box(FL_THIN_UP_BOX);
  box->labelfont(FL_TIMES_BOLD);
  box->labelsize(34);
  box->color(FL_WHITE);
  box->labelcolor(FL_BLUE);
  x += 50 + INNER_MARGIN;

  // Message area
  message = new Fl_Box(x, y, 340, 50);
  message->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);

  x += 340;
  y += 50 + INNER_MARGIN;

  // Buttons
  buttons = new Fl_Group(OUTER_MARGIN, y,
                         x - OUTER_MARGIN, BUTTON_HEIGHT);
  buttons->add_resizable(*(new Fl_Box(0, 0, 0, 0)));

  button = new Fl_Return_Button(x - BUTTON_WIDTH, y,
                                BUTTON_WIDTH, BUTTON_HEIGHT, fl_close);
  button->callback(button_cb, 0);

  buttons->end();

  end();
}

Fl_Message_Box_::~Fl_Message_Box_()
{
}

void Fl_Message_Box_::set_message(const char* fmt, va_list ap)
{
  char msg[1024];
  int msg_w, msg_h;

  vsnprintf(msg, sizeof(msg), fmt, ap);

  // Measure message size
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  msg_w = msg_h = 0;
  fl_measure(msg, msg_w, msg_h);
  // FLTK adds some arbitrary padding for the label
  msg_w += 6;

  if (msg_w < 340)
    msg_w = 340;
  if (msg_h < 50)
    msg_h = 50;

  message->size(msg_w, msg_h);
  message->copy_label(msg);

  buttons->resize(OUTER_MARGIN, message->y() + msg_h + INNER_MARGIN,
                  50 + INNER_MARGIN + msg_w, BUTTON_HEIGHT);

  size(OUTER_MARGIN * 2 + 50 + INNER_MARGIN + msg_w,
       OUTER_MARGIN * 2 + msg_h + INNER_MARGIN + BUTTON_HEIGHT);
}

Fl_Message_Box::Fl_Message_Box(const char* title, const char* fmt, ...)
  : Fl_Message_Box_(title, "i")
{
  va_list ap;

  va_start(ap, fmt);
  set_message(fmt, ap);
  va_end(ap);
}

Fl_Message_Box::~Fl_Message_Box()
{
}

Fl_Alert_Box::Fl_Alert_Box(const char* title, const char* fmt, ...)
  : Fl_Message_Box_(title, "!")
{
  va_list ap;

  va_start(ap, fmt);
  set_message(fmt, ap);
  va_end(ap);
}

Fl_Alert_Box::~Fl_Alert_Box()
{
}
