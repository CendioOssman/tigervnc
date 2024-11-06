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

#ifndef __FL_MESSAGE_BOX_H__
#define __FL_MESSAGE_BOX_H__

#include <stdarg.h>

#include <FL/Fl_Window.H>

class Fl_Box;
class Fl_Group;

class Fl_Message_Box_ : public Fl_Window
{
public:
  void finished(Fl_Callback* cb, void* p=nullptr);

  void hide() override;

protected:
  Fl_Message_Box_(const char* title, const char* icon,
                  const char* b0, const char* b1, const char* b2);
  virtual ~Fl_Message_Box_();

  void set_message(const char* fmt, va_list ap);

protected:
  int result_;

private:
  static void button_cb(Fl_Widget *w, long val);

private:
  Fl_Box* message;
  Fl_Group* buttons;

  Fl_Callback* callback_;
  void* user_data_;
};

class Fl_Message_Box : public Fl_Message_Box_
{
public:
  Fl_Message_Box(const char* title, const char* fmt, ...);
  virtual ~Fl_Message_Box();
};

class Fl_Alert_Box : public Fl_Message_Box_
{
public:
  Fl_Alert_Box(const char* title, const char* fmt, ...);
  virtual ~Fl_Alert_Box();
};

class Fl_Choice_Box : public Fl_Message_Box_
{
public:
  Fl_Choice_Box(const char* title, const char* fmt,
                const char* b0, const char* b1, const char* b2, ...);
  virtual ~Fl_Choice_Box();

  int result();
};

#endif
