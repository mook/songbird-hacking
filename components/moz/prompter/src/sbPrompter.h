/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 :miv */
/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
// http://songbirdnest.com
//
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the "GPL").
//
// Software distributed under the License is distributed
// on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
// express or implied. See the GPL for the specific language
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// END SONGBIRD GPL
//
*/

#ifndef __SB_PROMPTER_H__
#define __SB_PROMPTER_H__

/** 
 * \file  sbPrompter.h
 * \brief Songbird Prompter Definitions.
 */

// Songbird imports.
#include <sbIPrompter.h>

// Mozilla imports.
#include <nsCOMPtr.h>
#include <nsIDOMWindowInternal.h>
#include <nsIObserver.h>
#include <nsIWindowMediator.h>
#include <nsIWindowWatcher.h>
#include <nsStringAPI.h>


/**
 * This class implements the Songbird prompter interface.
 */

class sbPrompter : public sbIPrompter,
                   public nsIObserver
{
  //----------------------------------------------------------------------------
  //
  // Public interface.
  //
  //----------------------------------------------------------------------------

public:

  //
  // Inherited interfaces.
  //

  NS_DECL_ISUPPORTS
  NS_DECL_SBIPROMPTER
  NS_DECL_NSIPROMPTSERVICE
  NS_DECL_NSIOBSERVER


  //
  // Songbird prompter services.
  //

  sbPrompter();

  virtual ~sbPrompter();

  nsresult Init();


  //----------------------------------------------------------------------------
  //
  // Private interface.
  //
  //----------------------------------------------------------------------------

private:

  //
  // mPrompterLock               Prompter lock.
  // mWindowMediator             Window mediator service.
  // mWindowWatcher              Window watcher service.
  // mPromptService              Prompt service.
  // mParentWindowType           Parent window type.
  // mWaitForWindow              If true, wait for parent window type.
  //
  // The following fields must only be accessed under the prompter lock:
  //   mParentWindowType
  //   mWaitForWindow
  //

  PRLock*                        mPrompterLock;
  nsCOMPtr<nsIWindowMediator>    mWindowMediator;
  nsCOMPtr<nsIWindowWatcher>     mWindowWatcher;
  nsCOMPtr<nsIPromptService>     mPromptService;
  nsString                       mParentWindowType;
  PRBool                         mWaitForWindow;


  //
  // Internal Songbird prompter services.
  //

  nsresult InitOnMainThread();

  nsresult GetParent(nsIDOMWindow** aParent);

  nsresult GetProxiedPrompter(sbIPrompter** aPrompter);
};


//
// Songbird prompter component defs.
//

#define SONGBIRD_PROMPTER_CONTRACTID "@songbirdnest.com/Songbird/Prompter;1"
#define SONGBIRD_PROMPTER_CLASSNAME "Songbird Prompter"
#define SONGBIRD_PROMPTER_CID \
  /* {fa7ec5bd-7cab-4a63-a970-7bc4e83ee891} */ \
  { \
    0xfa7ec5bd, \
    0x7cab, \
    0x4a63, \
    { 0xa9, 0x70, 0x7b, 0xc4, 0xe8, 0x3e, 0xe8, 0x91 } \
  }

#endif // __SB_PROMPTER_H__

