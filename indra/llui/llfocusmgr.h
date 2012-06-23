/** 
 * @file llfocusmgr.h
 * @brief LLFocusMgr base class
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

// Singleton that manages keyboard and mouse focus

#ifndef LL_LLFOCUSMGR_H
#define LL_LLFOCUSMGR_H

#include "llstring.h"
#include "llframetimer.h"
#include "llui.h"

class LLUICtrl;
class LLMouseHandler;
class LLView;

class LLFocusableElement
{
	friend class LLFocusMgr; // allow access to focus change handlers
public:
	LLFocusableElement();
	virtual ~LLFocusableElement();

	virtual void	setFocus(BOOL b);
	virtual BOOL	hasFocus() const;

	void			setFocusLostCallback(void (*cb)(LLFocusableElement*, void*),
										 void* user_data = NULL);
	void			setFocusReceivedCallback(void (*cb)(LLFocusableElement*, void*),
											 void* user_data = NULL);
	void			setFocusChangedCallback(void (*cb)(LLFocusableElement*, void*),
											void* user_data = NULL);

	// These were brought up the hierarchy from LLView so that we don't have to
	// use dynamic_cast when dealing with keyboard focus.
	virtual BOOL	handleKey(KEY key, MASK mask, BOOL called_from_parent);
	virtual BOOL	handleUnicodeChar(llwchar uni_char, BOOL called_from_parent);

protected:
	virtual void	onFocusReceived();
	virtual void	onFocusLost();
	void			(*mFocusLostCallback)(LLFocusableElement* caller, void* userdata);
	void			(*mFocusReceivedCallback)(LLFocusableElement* ctrl, void* userdata);
	void			(*mFocusChangedCallback)(LLFocusableElement* ctrl, void* userdata);
	void*			mFocusCallbackUserData;
};

class LLFocusMgr
{
public:
	LLFocusMgr();
	~LLFocusMgr()												{ mFocusHistory.clear(); }

	void			setFocusColor(LLColor4 color)				{ mFocusColor = color; }

	// Mouse Captor

	// new_captor = NULL to release the mouse:
	void			setMouseCapture(LLMouseHandler* new_captor);
	LLMouseHandler* getMouseCapture() const						{ return mMouseCaptor; } 
	void			removeMouseCaptureWithoutCallback(const LLMouseHandler* captor);
	BOOL			childHasMouseCapture(const LLView* parent) const;

	// Keyboard Focus

	// new_focus = NULL to release the focus:
	void			setKeyboardFocus(LLFocusableElement* new_focus,
									 BOOL lock = FALSE,
									 BOOL keystrokes_only = FALSE);
	LLFocusableElement*	getKeyboardFocus() const				{ return mKeyboardFocus; }  
	LLFocusableElement*	getLastKeyboardFocus() const			{ return mLastKeyboardFocus; }  
	BOOL			childHasKeyboardFocus(const LLView* parent) const;
	void			removeKeyboardFocusWithoutCallback(const LLFocusableElement* focus);
	BOOL			getKeystrokesOnly()							{ return mKeystrokesOnly; }
	void			setKeystrokesOnly(BOOL keystrokes_only)		{ mKeystrokesOnly = keystrokes_only; }

	F32				getFocusTime() const						{ return mFocusTimer.getElapsedTimeF32(); }
	F32				getFocusFlashAmt() const;
	S32				getFocusFlashWidth() const					{ return llround(lerp(1.f, 3.f, getFocusFlashAmt())); }
	LLColor4		getFocusColor() const;
	void			triggerFocusFlash();
	BOOL			getAppHasFocus() const						{ return mAppHasFocus; }
	void			setAppHasFocus(BOOL focus);
	LLUICtrl*		getLastFocusForGroup(LLView* subtree_root) const;
	void			clearLastFocusForGroup(LLView* subtree_root);

	// If setKeyboardFocus(NULL) is called, and there is a non-NULL default
	// keyboard focus view, focus goes there. JC
	void			setDefaultKeyboardFocus(LLFocusableElement* default_focus) { mDefaultKeyboardFocus = default_focus; }
	LLFocusableElement*	getDefaultKeyboardFocus() const			{ return mDefaultKeyboardFocus; }

	// Top View

	void			setTopCtrl(LLUICtrl* new_top);
	LLUICtrl*		getTopCtrl() const							{ return mTopCtrl; }
	void			removeTopCtrlWithoutCallback(const LLUICtrl* top_view);
	BOOL			childIsTopCtrl(const LLView* parent) const;

	// All Three

	void			releaseFocusIfNeeded(const LLView* top_view);
	void			lockFocus();
	void			unlockFocus();
	BOOL			focusLocked() const							{ return mLockedView != NULL; }

private:
	LLColor4			mFocusColor;

	LLUICtrl*			mLockedView;

	// Mouse Captor

	// Mouse events are premptively routed to this object
	LLMouseHandler*		mMouseCaptor;

	// Keyboard Focus

	// Keyboard events are preemptively routed to this object:
	LLFocusableElement*	mKeyboardFocus;
	LLFocusableElement*	mLastKeyboardFocus;		// who last had focus
	LLFocusableElement*	mDefaultKeyboardFocus;
	BOOL				mKeystrokesOnly;

	// Top View
	LLUICtrl*			mTopCtrl;

	LLFrameTimer		mFocusTimer;
	F32					mFocusWeight;

	BOOL				mAppHasFocus;

	typedef std::map<LLHandle<LLView>, LLHandle<LLView> > focus_history_map_t;
	focus_history_map_t mFocusHistory;

#ifdef _DEBUG
	std::string			mMouseCaptorName;
	std::string			mKeyboardFocusName;
	std::string			mTopCtrlName;
#endif
};

extern LLFocusMgr gFocusMgr;

#endif  // LL_LLFOCUSMGR_H
