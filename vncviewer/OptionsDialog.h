/* Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifndef __OPTIONSDIALOG_H__
#define __OPTIONSDIALOG_H__

#include <list>
#include <map>

#include <FL/Fl_Window.H>

class OptionsPage;

typedef void (OptionsCallback)(void*);

class OptionsDialog : public Fl_Window {
public:
  OptionsDialog();
  ~OptionsDialog();

  static void addCallback(OptionsCallback *cb, void *data = nullptr);
  static void removeCallback(OptionsCallback *cb);

  void finished(Fl_Callback* cb, void* p=nullptr);

  void hide(void) override;

protected:
  void loadOptions(void);
  void storeOptions(void);

  void handleCancel();
  void handleOK();

protected:
  static std::map<OptionsCallback*, void*> callbacks;

  std::list<OptionsPage*> pages;

  Fl_Callback* finishedCallback;
  void* finishedUserData;
};

#endif
