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

#include <assert.h>
#include <stdarg.h>

#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
#include <FL/fl_ask.H>

#include "Fl_Message_Box.h"
#include "layout.h"

Fl_Message_Box_::Fl_Message_Box_(const char* title, const char* icon,
                                 const char* b0, const char* b1,
                                 const char* b2)
  : Fl_Window(0, 0, title), result_(0),
    callback_(nullptr), user_data_(nullptr)
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

  if (b0 != nullptr) {
    button = new Fl_Button(x - BUTTON_WIDTH, y,
                           BUTTON_WIDTH, BUTTON_HEIGHT);
    button->copy_label(b0);
    button->callback(button_cb, 0);
    x -= BUTTON_WIDTH + INNER_MARGIN;
  }
  if (b1 != nullptr) {
    button = new Fl_Return_Button(x - BUTTON_WIDTH, y,
                                  BUTTON_WIDTH, BUTTON_HEIGHT);
    button->copy_label(b1);
    button->callback(button_cb, 1);
    x -= BUTTON_WIDTH + INNER_MARGIN;
  }
  if (b2 != nullptr) {
    button = new Fl_Button(x - BUTTON_WIDTH, y,
                           BUTTON_WIDTH, BUTTON_HEIGHT);
    button->copy_label(b2);
    button->callback(button_cb, 2);
    x -= BUTTON_WIDTH + INNER_MARGIN;
  }

  buttons->end();

  end();
}

Fl_Message_Box_::~Fl_Message_Box_()
{
}

void Fl_Message_Box_::finished(Fl_Callback* cb, void* p)
{
  callback_ = cb;
  user_data_ = p;
}

void Fl_Message_Box_::hide()
{
  if (callback_ != nullptr)
    callback_(this, user_data_);
  Fl_Window::hide();
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

void Fl_Message_Box_::button_cb(Fl_Widget *w, long val)
{
  Fl_Message_Box_* self;

  self = dynamic_cast<Fl_Message_Box_*>(w->window());
  assert(self != nullptr);

  self->result_ = val;
  self->hide();
}

Fl_Message_Box::Fl_Message_Box(const char* title, const char* fmt, ...)
  : Fl_Message_Box_(title, "i", nullptr, fl_close, nullptr)
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
  : Fl_Message_Box_(title, "!", nullptr, fl_close, nullptr)
{
  va_list ap;

  va_start(ap, fmt);
  set_message(fmt, ap);
  va_end(ap);
}

Fl_Alert_Box::~Fl_Alert_Box()
{
}

Fl_Choice_Box::Fl_Choice_Box(const char* title, const char* fmt,
                             const char* b0, const char* b1,
                             const char* b2, ...)
  : Fl_Message_Box_(title, "?", b0, b1, b2)
{
  va_list ap;

  va_start(ap, b2);
  set_message(fmt, ap);
  va_end(ap);
}

Fl_Choice_Box::~Fl_Choice_Box()
{
}

int Fl_Choice_Box::result()
{
  return result_;
}
