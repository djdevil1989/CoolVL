/** 
 * @file llpreviewscript.cpp
 * @brief LLPreviewScript class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llpreviewscript.h"

#include "llassetstorage.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llresmgr.h"
#include "llscrolllistctrl.h"
#include "llsecondlifeurls.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llvfile.h"
#include "lscript_rt_interface.h"
#include "lscript_export.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llassetuploadresponders.h"
#include "lldirpicker.h"
#include "llfloatersearchreplace.h"
#include "llinventorymodel.h"
#include "llmediactrl.h"
#include "llpanelinventory.h"
#include "llselectmgr.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "llviewernetwork.h"				// for gIsInSecondLife
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
//MK
#include "llvoavatar.h"
//mk

const std::string HELLO_LSL =
	"default {\n"
	"    state_entry() {\n"
    "        llOwnerSay(\"New script: Hello, Avatar!\");\n"
    "    }\n"
	"\n"
	"    touch_start(integer total_number) {\n"
	"        llWhisper(0, \"New script: Touched.\");\n"
	"    }\n"
	"}\n";
// Description and header information
const std::string DEFAULT_SCRIPT_NAME = "New Script"; // *TODO:Translate?
const std::string DEFAULT_SCRIPT_DESC = "(No Description)"; // *TODO:Translate?

const S32 SCRIPT_BORDER = 4;
const S32 SCRIPT_PAD = 5;
const S32 SCRIPT_BUTTON_WIDTH = 128;
const S32 SCRIPT_BUTTON_HEIGHT = 24;	// HACK: Use BTN_HEIGHT where possible.
const S32 LINE_COLUMN_HEIGHT = 14;
const S32 BTN_PAD = 8;
const S32 SCRIPT_EDITOR_MIN_HEIGHT = 2 * SCROLLBAR_SIZE + 2 * LLPANEL_BORDER_WIDTH + 128;
const S32 SCRIPT_MIN_WIDTH = 2 * SCRIPT_BORDER + 2 * SCRIPT_BUTTON_WIDTH + 
							 SCRIPT_PAD + RESIZE_HANDLE_WIDTH + SCRIPT_PAD;
const S32 SCRIPT_MIN_HEIGHT = 2 * SCRIPT_BORDER + 3 * (SCRIPT_BUTTON_HEIGHT + SCRIPT_PAD) +
							  LINE_COLUMN_HEIGHT + SCRIPT_EDITOR_MIN_HEIGHT;
const S32 MAX_EXPORT_SIZE = 1000;
const S32 TEXT_EDIT_COLUMN_HEIGHT = 16;
const S32 MAX_HISTORY_COUNT = 10;
const F32 LIVE_HELP_REFRESH_TIME = 1.f;

static bool have_script_upload_cap(LLUUID object_id)
{
	LLViewerRegion* region = NULL;
	if (object_id == LLUUID::null)
	{
		region = gAgent.getRegion();
	}
	else
	{
		LLViewerObject* object = gObjectList.findObject(object_id);
		if (object)
		{
			region = object->getRegion();
		}
	}
	return region && !region->getCapability("UpdateScriptTask").empty();
}

/// ---------------------------------------------------------------------------
/// LLScriptEdCore
/// ---------------------------------------------------------------------------

std::set<LLScriptEdCore*> LLScriptEdCore::sList;

struct LLSECKeywordCompare
{
	bool operator()(const std::string& lhs, const std::string& rhs)
	{
		return (LLStringUtil::compareDictInsensitive(lhs, rhs) < 0);
	}
};

LLScriptEdCore::LLScriptEdCore(const std::string& name,
							   const LLRect& rect,
							   const std::string& sample,
							   const std::string& help_url,
							   const LLHandle<LLFloater>& floater_handle,
							   void (*load_callback)(void*),
							   void (*save_callback)(void*, BOOL),
							   void (*search_replace_callback) (void* userdata),
							   void* userdata,
							   S32 bottom_pad)
:	LLPanel(std::string("name"), rect),
	mScriptName(std::string("untitled")),
	mSampleText(sample),
	mHelpURL(help_url),
	mEditor(NULL),
	mMonoCheckbox(NULL),
	mLoadCallback(load_callback),
	mSaveCallback(save_callback),
	mSearchReplaceCallback(search_replace_callback),
	mUserdata(userdata),
	mForceClose(FALSE),
	mLastHelpToken(NULL),
	mLiveHelpHistorySize(0),
	mEnableSave(FALSE),
	mHasScriptData(FALSE),
	LLEventTimer(60)
{
	sList.insert(this);

	setFollowsAll();
	setBorderVisible(FALSE);

	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "floater_script_ed_panel.xml");

	mErrorList = getChild<LLScrollListCtrl>("lsl errors");

	mFunctions = getChild<LLComboBox>("Insert...");

	childSetCommitCallback("Insert...", &LLScriptEdCore::onBtnInsertFunction, this);

	mEditor = getChild<LLViewerTextEditor>("Script Editor");
	mEditor->setFollowsAll();
	mEditor->setHandleEditKeysDirectly(TRUE);
	mEditor->setEnabled(TRUE);
	mEditor->setWordWrap(TRUE);

	mMonoCheckbox =	getChild<LLCheckBoxCtrl>("mono");
	childSetCommitCallback("mono", &LLScriptEdCore::onMonoCheckboxClicked, this);
	childSetEnabled("mono", false);

	std::vector<std::string> funcs;
	std::vector<std::string> tooltips;
	for (std::vector<LLScriptLibraryFunction>::const_iterator i = gScriptLibrary.mFunctions.begin();
		 i != gScriptLibrary.mFunctions.end(); ++i)
	{
		// Make sure this isn't a god only function, or the agent is a god.
		if (!i->mGodOnly || gAgent.isGodlike())
		{
			std::string name = i->mName;
			funcs.push_back(name);

			std::string desc_name = "LSLTipText_";
			desc_name += name;
			std::string desc = LLTrans::getString(desc_name);

			F32 sleep_time = i->mSleepTime;
			if (sleep_time)
			{
				desc += "\n";

				LLStringUtil::format_map_t args;
				args["[SLEEP_TIME]"] = llformat("%.1f", sleep_time);
				desc += LLTrans::getString("LSLTipSleepTime", args);
			}

			// A \n linefeed is not part of xml. Let's add one to keep all
			// the tips one-per-line in strings.xml
			LLStringUtil::replaceString(desc, "\\n", "\n");

			tooltips.push_back(desc);
		}
	}

	LLColor3 color(0.5f, 0.0f, 0.15f);
	mEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "keywords.ini"), funcs, tooltips, color);

	std::vector<std::string> primary_keywords;
	std::vector<std::string> secondary_keywords;
	LLKeywordToken *token;
	LLKeywords::keyword_iterator_t token_it;
	for (token_it = mEditor->keywordsBegin(); token_it != mEditor->keywordsEnd(); ++token_it)
	{
		token = token_it->second;
		if (token->getColor() == color) // Wow, what a disgusting hack.
		{
			primary_keywords.push_back(wstring_to_utf8str(token->getToken()));
		}
		else
		{
			secondary_keywords.push_back(wstring_to_utf8str(token->getToken()));
		}
	}

	// Case-insensitive dictionary sort for primary keywords. We don't sort the secondary
	// keywords. They're intelligently grouped in keywords.ini.
	std::stable_sort(primary_keywords.begin(), primary_keywords.end(), LLSECKeywordCompare());

	for (std::vector<std::string>::const_iterator iter= primary_keywords.begin();
			iter!= primary_keywords.end(); ++iter)
	{
		mFunctions->add(*iter);
	}

	for (std::vector<std::string>::const_iterator iter= secondary_keywords.begin();
			iter!= secondary_keywords.end(); ++iter)
	{
		mFunctions->add(*iter);
	}
 
	childSetCommitCallback("lsl errors", &LLScriptEdCore::onErrorList, this);

	mSaveButton = getChild<LLButton>("Save_btn");
	mSaveButton->setClickedCallback(onBtnSave, this);

	mLineColText = getChild<LLTextBox>("line_col");

	initMenu();

	// Do the work that addTabPanel() normally does.
	//LLRect tab_panel_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	//tab_panel_rect.stretch(-LLPANEL_BORDER_WIDTH);
	//mCodePanel->setFollowsAll();
	//mCodePanel->translate(tab_panel_rect.mLeft - mCodePanel->getRect().mLeft, tab_panel_rect.mBottom - mCodePanel->getRect().mBottom);
	//mCodePanel->reshape(tab_panel_rect.getWidth(), tab_panel_rect.getHeight(), TRUE);
}

LLScriptEdCore::~LLScriptEdCore()
{
	deleteBridges();
	sList.erase(this);
}

BOOL LLScriptEdCore::tick()
{
	autoSave();
	return FALSE;
}

void LLScriptEdCore::initMenu()
{

	LLMenuItemCallGL* menuItem = getChild<LLMenuItemCallGL>("Load From File");
	menuItem->setMenuCallback(onBtnLoadFromFile, this);
	menuItem->setEnabledCallback(enableSaveLoadFile);

	menuItem = getChild<LLMenuItemCallGL>("Save To File");
	menuItem->setMenuCallback(onBtnSaveToFile, this);
	menuItem->setEnabledCallback(enableSaveLoadFile);

	menuItem = getChild<LLMenuItemCallGL>("Save To Inventory");
	menuItem->setMenuCallback(onBtnSave, this);
	menuItem->setEnabledCallback(hasChanged);

	menuItem = getChild<LLMenuItemCallGL>("Revert All Changes");
	menuItem->setMenuCallback(onBtnUndoChanges, this);
	menuItem->setEnabledCallback(hasChanged);

	menuItem = getChild<LLMenuItemCallGL>("Undo");
	menuItem->setMenuCallback(onUndoMenu, this);
	menuItem->setEnabledCallback(enableUndoMenu);

	menuItem = getChild<LLMenuItemCallGL>("Redo");
	menuItem->setMenuCallback(onRedoMenu, this);
	menuItem->setEnabledCallback(enableRedoMenu);

	menuItem = getChild<LLMenuItemCallGL>("Cut");
	menuItem->setMenuCallback(onCutMenu, this);
	menuItem->setEnabledCallback(enableCutMenu);

	menuItem = getChild<LLMenuItemCallGL>("Copy");
	menuItem->setMenuCallback(onCopyMenu, this);
	menuItem->setEnabledCallback(enableCopyMenu);

	menuItem = getChild<LLMenuItemCallGL>("Paste");
	menuItem->setMenuCallback(onPasteMenu, this);
	menuItem->setEnabledCallback(enablePasteMenu);

	menuItem = getChild<LLMenuItemCallGL>("Select All");
	menuItem->setMenuCallback(onSelectAllMenu, this);
	menuItem->setEnabledCallback(enableSelectAllMenu);

	menuItem = getChild<LLMenuItemCallGL>("Deselect");
	menuItem->setMenuCallback(onDeselectMenu, this);
	menuItem->setEnabledCallback(enableDeselectMenu);

	menuItem = getChild<LLMenuItemCallGL>("Search / Replace...");
	menuItem->setMenuCallback(onSearchMenu, this);
	menuItem->setEnabledCallback(NULL);

	menuItem = getChild<LLMenuItemCallGL>("Help...");
	menuItem->setMenuCallback(onBtnHelp, this);
	menuItem->setEnabledCallback(NULL);

	menuItem = getChild<LLMenuItemCallGL>("LSL Wiki Help...");
	menuItem->setMenuCallback(onBtnDynamicHelp, this);
	menuItem->setEnabledCallback(NULL);
}

void LLScriptEdCore::setScriptText(const std::string& text, BOOL is_valid)
{
	if (mEditor)
	{
		mEditor->setText(text);
		mHasScriptData = is_valid;
	}
}

void LLScriptEdCore::setScriptName(std::string name)
{
	if (name.find("Script: ") == 0)
	{
		name = name.substr(8);
	}
	if (name.empty())
	{
		name = "untitled";
	}
	mScriptName = name;
}

BOOL LLScriptEdCore::hasChanged(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;

	return ((!self->mEditor->isPristine() || self->mEnableSave) && self->mHasScriptData);
}

static const F32 UPDATE_FREQUENCY = 0.5f;

void LLScriptEdCore::draw()
{
	BOOL script_changed	= hasChanged(this);
	mSaveButton->setEnabled(script_changed);

	if (mEditor->hasFocus())
	{
		S32 line = 0;
		S32 column = 0;
		mEditor->getCurrentLineAndColumn(&line, &column, FALSE);  // don't include wordwrap
		std::string cursor_pos;
		cursor_pos = llformat("Line %d, Column %d", line, column);
		mLineColText->setText(cursor_pos);
	}
	else
	{
		mLineColText->setText(LLStringUtil::null);
	}

	static LLTimer update_timer;
	if (update_timer.hasExpired())
	{
		// Don't do this every frame !
		updateDynamicHelp();
		update_timer.setTimerExpirySec(UPDATE_FREQUENCY);
	}

	LLPanel::draw();
}

void LLScriptEdCore::updateDynamicHelp(BOOL immediate)
{
	LLFloater* help_floater = mLiveHelpHandle.get();
	if (!help_floater || ! help_floater->getVisible()) return;

	// update back and forward buttons
	LLButton* fwd_button = help_floater->getChild<LLButton>("fwd_btn");
	LLButton* back_button = help_floater->getChild<LLButton>("back_btn");
	LLMediaCtrl* browser = help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
	back_button->setEnabled(browser->canNavigateBack());
	fwd_button->setEnabled(browser->canNavigateForward());

	static LLCachedControl<bool> help_follow_cursor(gSavedSettings,
													"ScriptHelpFollowsCursor");
	if (!immediate && !help_follow_cursor)
	{
		return;
	}

	const LLTextSegment* segment = NULL;
	std::vector<const LLTextSegment*> selected_segments;
	mEditor->getSelectedSegments(selected_segments);

	// try segments in selection range first
	for (std::vector<const LLTextSegment*>::iterator
			segment_iter = selected_segments.begin(),
			end = selected_segments.end();
		 segment_iter != end; ++segment_iter)
	{
		if ((*segment_iter)->getToken() &&
			(*segment_iter)->getToken()->getType() == LLKeywordToken::WORD)
		{
			segment = *segment_iter;
			break;
		}
	}

	// then try previous segment in case we just typed it
	if (!segment)
	{
		const LLTextSegment* test_segment = mEditor->getPreviousSegment();
		if (test_segment->getToken() &&
			test_segment->getToken()->getType() == LLKeywordToken::WORD)
		{
			segment = test_segment;
		}
	}

	if (segment)
	{
		if (segment->getToken() != mLastHelpToken)
		{
			mLastHelpToken = segment->getToken();
			mLiveHelpTimer.start();
		}
		if (immediate ||
			(mLiveHelpTimer.getStarted() &&
			 mLiveHelpTimer.getElapsedTimeF32() > LIVE_HELP_REFRESH_TIME))
		{
			std::string help_string = mEditor->getText().substr(segment->getStart(),
																segment->getEnd() - segment->getStart());
			setHelpPage(help_string);
			mLiveHelpTimer.stop();
		}
	}
	else if (immediate)
	{
		setHelpPage(LLStringUtil::null);
	}
}

void LLScriptEdCore::autoSave()
{
	//llinfos << "LLScriptEdCore::autoSave()" << llendl;
	if (mEditor->isPristine())
	{
		return;
	}
	//std::string filepath = gDirUtilp->getExpandedFilename(gDirUtilp->getTempDir(),asset_id.asString());
	if (mAutosaveFilename.empty()) {
		std::string asfilename = gDirUtilp->getTempFilename();
		asfilename.replace(asfilename.length()-4, 12, "_autosave.lsl");
		mAutosaveFilename = asfilename;
		//mAutosaveFilename = llformat("%s.lsl", asfilename.c_str());
	}

	FILE* fp = LLFile::fopen(mAutosaveFilename.c_str(), "wb");
	if (!fp)
	{
		llwarns << "Unable to write to " << mAutosaveFilename << llendl;

		LLSD row;
		row["columns"][0]["value"] = "Error writing to temp file. Is your hard drive full?";
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		mErrorList->addElement(row);
		return;
	}

	std::string utf8text = mEditor->getText();
	fputs(utf8text.c_str(), fp);
	fclose(fp);
	fp = NULL;
	llinfos << "autosave: " << mAutosaveFilename << llendl;
}

void LLScriptEdCore::setHelpPage(const std::string& help_string)
{
	LLFloater* help_floater = mLiveHelpHandle.get();
	if (!help_floater) return;

	LLMediaCtrl* web_browser = help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
	if (!web_browser) return;

	LLComboBox* history_combo = help_floater->getChild<LLComboBox>("history_combo");
	if (!history_combo) return;

	LLUIString url_string = gSavedSettings.getString("LSLHelpURL");
	std::string topic = help_string;
	if (topic.empty())
	{
		topic = gSavedSettings.getString("LSLHelpDefaultTopic");
	}
	url_string.setArg("[LSL_STRING]", topic);

	addHelpItemToHistory(help_string);

	web_browser->navigateTo(url_string);
}

void LLScriptEdCore::addHelpItemToHistory(const std::string& help_string)
{
	if (help_string.empty()) return;

	LLFloater* help_floater = mLiveHelpHandle.get();
	if (!help_floater) return;

	LLComboBox* history_combo = help_floater->getChild<LLComboBox>("history_combo");
	if (!history_combo) return;

	// separate history items from full item list
	if (mLiveHelpHistorySize == 0)
	{
		LLSD row;
		row["columns"][0]["type"] = "separator";
		history_combo->addElement(row, ADD_TOP);
	}
	// delete all history items over history limit
	while (mLiveHelpHistorySize > MAX_HISTORY_COUNT - 1)
	{
		history_combo->remove(mLiveHelpHistorySize - 1);
		mLiveHelpHistorySize--;
	}

	history_combo->setSimple(help_string);
	S32 index = history_combo->getCurrentIndex();

	// if help string exists in the combo box
	if (index >= 0)
	{
		S32 cur_index = history_combo->getCurrentIndex();
		if (cur_index < mLiveHelpHistorySize)
		{
			// item found in history, bubble up to top
			history_combo->remove(history_combo->getCurrentIndex());
			mLiveHelpHistorySize--;
		}
	}
	history_combo->add(help_string, LLSD(help_string), ADD_TOP);
	history_combo->selectFirstItem();
	mLiveHelpHistorySize++;
}

BOOL LLScriptEdCore::canClose()
{
	if (mForceClose || !hasChanged(this))
	{
		return TRUE;
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotifications::instance().add("SaveChanges", LLSD(), LLSD(), boost::bind(&LLScriptEdCore::handleSaveChangesDialog, this, _1, _2));
		return FALSE;
	}
}

bool LLScriptEdCore::handleSaveChangesDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch(option)
	{
	case 0:  // "Yes"
		// close after saving
		LLScriptEdCore::doSave(this, TRUE);
		break;

	case 1:  // "No"
		if (!mAutosaveFilename.empty()) 
		{
			llinfos << "remove autosave: " << mAutosaveFilename << llendl;
			LLFile::remove(mAutosaveFilename.c_str());
		}
		mForceClose = TRUE;
		// This will close immediately because mForceClose is true, so we won't
		// infinite loop with these dialogs. JC
		((LLFloater*) getParent())->close();
		break;

	case 2: // "Cancel"
	default:
		// If we were quitting, we didn't really mean it.
        LLAppViewer::instance()->abortQuit();
		break;
	}
	return false;
}

// static 
bool LLScriptEdCore::onHelpWebDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	switch(option)
	{
	case 0:
		LLWeb::loadURL(notification["payload"]["help_url"]);
		break;
	default:
		break;
	}
	return false;
}

// static 
void LLScriptEdCore::onBtnHelp(void* userdata)
{
	LLScriptEdCore* corep = (LLScriptEdCore*)userdata;
	LLSD payload;
	payload["help_url"] = corep->mHelpURL;
	LLNotifications::instance().add("WebLaunchLSLGuide", LLSD(), payload,
									onHelpWebDialog);
}

// static 
void LLScriptEdCore::onBtnDynamicHelp(void* userdata)
{
	LLScriptEdCore* corep = (LLScriptEdCore*)userdata;

	LLFloater* live_help_floater = corep->mLiveHelpHandle.get();
	if (live_help_floater)
	{
		live_help_floater->setFocus(TRUE);
		corep->updateDynamicHelp(TRUE);
		return;
	}

	live_help_floater = new LLFloater(std::string("lsl_help"));
	LLUICtrlFactory::getInstance()->buildFloater(live_help_floater, "floater_lsl_guide.xml");
	((LLFloater*)corep->getParent())->addDependentFloater(live_help_floater, TRUE);
	live_help_floater->childSetCommitCallback("lock_check", onCheckLock, userdata);
	live_help_floater->childSetValue("lock_check", gSavedSettings.getBOOL("ScriptHelpFollowsCursor"));
	live_help_floater->childSetCommitCallback("history_combo", onHelpComboCommit, userdata);
	live_help_floater->childSetAction("back_btn", onClickBack, userdata);
	live_help_floater->childSetAction("fwd_btn", onClickForward, userdata);

	LLMediaCtrl* browser = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
	browser->setAlwaysRefresh(true);

	LLComboBox* help_combo = live_help_floater->getChild<LLComboBox>("history_combo");
	LLKeywordToken *token;
	LLKeywords::keyword_iterator_t token_it;
	for (token_it = corep->mEditor->keywordsBegin(); 
		 token_it != corep->mEditor->keywordsEnd(); ++token_it)
	{
		token = token_it->second;
		help_combo->add(wstring_to_utf8str(token->getToken()));
	}
	help_combo->sortByName();

	// re-initialize help variables
	corep->mLastHelpToken = NULL;
	corep->mLiveHelpHandle = live_help_floater->getHandle();
	corep->mLiveHelpHistorySize = 0;
	corep->updateDynamicHelp(TRUE);
}

//static 
void LLScriptEdCore::onClickBack(void* userdata)
{
	LLScriptEdCore* corep = (LLScriptEdCore*)userdata;
	LLFloater* live_help_floater = corep->mLiveHelpHandle.get();
	if (live_help_floater)
	{
		LLMediaCtrl* browserp = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
		if (browserp)
		{
			browserp->navigateBack();
		}
	}
}

//static 
void LLScriptEdCore::onClickForward(void* userdata)
{
	LLScriptEdCore* corep = (LLScriptEdCore*)userdata;
	LLFloater* live_help_floater = corep->mLiveHelpHandle.get();
	if (live_help_floater)
	{
		LLMediaCtrl* browserp = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
		if (browserp)
		{
			browserp->navigateForward();
		}
	}
}

// static
void LLScriptEdCore::onCheckLock(LLUICtrl* ctrl, void* userdata)
{
	LLScriptEdCore* corep = (LLScriptEdCore*)userdata;

	// clear out token any time we lock the frame, so we will refresh web page
	// immediately when unlocked
	gSavedSettings.setBOOL("ScriptHelpFollowsCursor", ctrl->getValue().asBoolean());

	corep->mLastHelpToken = NULL;
}

// static 
void LLScriptEdCore::onBtnInsertSample(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*) userdata;

	// Insert sample code
	self->mEditor->selectAll();
	self->mEditor->cut();
	self->mEditor->insertText(self->mSampleText);
}

// static 
void LLScriptEdCore::onHelpComboCommit(LLUICtrl* ctrl, void* userdata)
{
	LLScriptEdCore* corep = (LLScriptEdCore*)userdata;

	LLFloater* live_help_floater = corep->mLiveHelpHandle.get();
	if (live_help_floater)
	{
		std::string help_string = ctrl->getValue().asString();

		corep->addHelpItemToHistory(help_string);

		LLMediaCtrl* web_browser = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
		LLUIString url_string = gSavedSettings.getString("LSLHelpURL");
		url_string.setArg("[LSL_STRING]", help_string);
		web_browser->navigateTo(url_string);
	}
}

// static 
void LLScriptEdCore::onBtnInsertFunction(LLUICtrl *ui, void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*) userdata;

	// Insert sample code
	if (self->mEditor->getEnabled())
	{
		self->mEditor->insertText(self->mFunctions->getSimple());
	}
	self->mEditor->setFocus(TRUE);
	self->setHelpPage(self->mFunctions->getSimple());
}

// static 
void LLScriptEdCore::doSave(void* userdata, BOOL close_after_save)
{
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_LSL_SAVE_COUNT);

	LLScriptEdCore* self = (LLScriptEdCore*) userdata;

	if (self->mSaveCallback)
	{
		self->mSaveCallback(self->mUserdata, close_after_save);
	}
}

// static 
BOOL LLScriptEdCore::enableSaveLoadFile(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return !LLFilePickerThread::isInUse() && !LLDirPickerThread::isInUse()
		   && self->mHasScriptData;
}

// static
void LLScriptEdCore::loadFromFileCallback(LLFilePicker::ELoadFilter type,
										  std::string& filename,
										  std::deque<std::string>& files,
										  void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !sList.count(self))
	{
		LLNotifications::instance().add("LoadScriptAborted");
		return;
	}
	if (!filename.empty())
	{
		std::ifstream file(filename.c_str());
		if (!file.fail())
		{
			self->mEditor->clear();
			std::string line, text;
			while (!file.eof())
			{
				getline(file, line);
				text += line + "\n";
			}
			file.close();
			LLWString wtext = utf8str_to_wstring(text);
			LLWStringUtil::replaceTabsWithSpaces(wtext, 4);
			text = wstring_to_utf8str(wtext);
			self->setScriptText(text, TRUE);
			self->enableSave(TRUE);
		}
	}
}

// static
void LLScriptEdCore::onBtnLoadFromFile(void* userdata)
{
	(new LLLoadFilePicker(LLFilePicker::FFLOAD_SCRIPT,
						  LLScriptEdCore::loadFromFileCallback,
						  userdata))->getFile();
}

//static
void LLScriptEdCore::saveToFileCallback(LLFilePicker::ESaveFilter type,
										std::string& filename,
										void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !sList.count(self))
	{
		LLNotifications::instance().add("SaveScriptAborted");
		return;
	}

	if (!filename.empty())
	{
		std::string lcname = filename;
		LLStringUtil::toLower(lcname);
		if (lcname.find(".lsl") != lcname.length() - 4 &&
			lcname.find(".txt") != lcname.length() - 4)
		{
			filename += ".lsl";
		}
		std::ofstream file(filename.c_str());
		if (!file.fail())
		{
			file << self->mEditor->getText();
			file.close();
		}
	}
}

// static
void LLScriptEdCore::onBtnSaveToFile(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !sList.count(self)) return;
	std::string suggestion = self->mScriptName + ".lsl";
	(new LLSaveFilePicker(LLFilePicker::FFSAVE_SCRIPT,
						  LLScriptEdCore::saveToFileCallback,
						  userdata))->getSaveFile(suggestion);
}

// static
void LLScriptEdCore::onBtnSave(void* userdata)
{
	// do the save, but don't close afterwards
	doSave(userdata, FALSE);
}

// static
void LLScriptEdCore::onBtnUndoChanges(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*) userdata;
	if (!self->mEditor->tryToRevertToPristineState())
	{
		LLNotifications::instance().add("ScriptCannotUndo", LLSD(), LLSD(), boost::bind(&LLScriptEdCore::handleReloadFromServerDialog, self, _1, _2));
	}
}

void LLScriptEdCore::onSearchMenu(void* userdata)
{
	LLScriptEdCore* sec = (LLScriptEdCore*)userdata;
	if (sec && sec->mEditor)
	{
		LLFloaterSearchReplace::show(sec->mEditor);
	}
}

// static 
void LLScriptEdCore::onUndoMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return;
	self->mEditor->undo();
}

// static 
void LLScriptEdCore::onRedoMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return;
	self->mEditor->redo();
}

// static 
void LLScriptEdCore::onCutMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return;
	self->mEditor->cut();
}

// static 
void LLScriptEdCore::onCopyMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return;
	self->mEditor->copy();
}

// static 
void LLScriptEdCore::onPasteMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return;
	self->mEditor->paste();
}

// static 
void LLScriptEdCore::onSelectAllMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return;
	self->mEditor->selectAll();
}

// static 
void LLScriptEdCore::onDeselectMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return;
	self->mEditor->deselect();
}

// static 
BOOL LLScriptEdCore::enableUndoMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canUndo();
}

// static 
BOOL LLScriptEdCore::enableRedoMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canRedo();
}

// static 
BOOL LLScriptEdCore::enableCutMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canCut();
}

// static 
BOOL LLScriptEdCore::enableCopyMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canCopy();
}

// static 
BOOL LLScriptEdCore::enablePasteMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canPaste();
}

// static 
BOOL LLScriptEdCore::enableSelectAllMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canSelectAll();
}

// static 
BOOL LLScriptEdCore::enableDeselectMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canDeselect();
}

// static
void LLScriptEdCore::onErrorList(LLUICtrl*, void* user_data)
{
	LLScriptEdCore* self = (LLScriptEdCore*)user_data;
	LLScrollListItem* item = self->mErrorList->getFirstSelected();
	if (item)
	{
		// *FIX: This fucked up little hack is here because we don't
		// have a grep library. This is very brittle code.
		S32 row = 0;
		S32 column = 0;
		const LLScrollListCell* cell = item->getColumn(0);
		std::string line(cell->getValue().asString());
		line.erase(0, 1);
		LLStringUtil::replaceChar(line, ',',' ');
		LLStringUtil::replaceChar(line, ')',' ');
		sscanf(line.c_str(), "%d %d", &row, &column);
		//llinfos << "LLScriptEdCore::onErrorList() - " << row << ", "
		//<< column << llendl;
		self->mEditor->setCursor(row, column);
		self->mEditor->setFocus(TRUE);
	}
}

bool LLScriptEdCore::handleReloadFromServerDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch(option)
	{
	case 0: // "Yes"
		if (mLoadCallback)
		{
			setScriptText(getString("loading"), FALSE);
			mLoadCallback(mUserdata);
		}
		break;

	case 1: // "No"
		break;

	default:
		llassert(0);
		break;
	}
	return false;
}

void LLScriptEdCore::selectFirstError()
{
	// Select the first item;
	mErrorList->selectFirstItem();
	onErrorList(mErrorList, this);
}

struct LLEntryAndEdCore
{
	LLScriptEdCore* mCore;
	LLEntryAndEdCore(LLScriptEdCore* core) :
		mCore(core)
		{}
};

void LLScriptEdCore::deleteBridges()
{
	S32 count = mBridges.count();
	LLEntryAndEdCore* eandc;
	for (S32 i = 0; i < count; i++)
	{
		eandc = mBridges.get(i);
		delete eandc;
		mBridges[i] = NULL;
	}
	mBridges.reset();
}

// virtual
BOOL LLScriptEdCore::handleKeyHere(KEY key, MASK mask)
{
	bool just_control = MASK_CONTROL == (mask & MASK_MODIFIERS);

	if (('S' == key) && just_control)
	{
		if (mSaveCallback)
		{
			// don't close after saving
			mSaveCallback(mUserdata, FALSE);
		}

		return TRUE;
	}

	if (('F' == key) && just_control)
	{
		if (mSearchReplaceCallback)
		{
			mSearchReplaceCallback(mUserdata);
		}

		return TRUE;
	}

	return FALSE;
}

void LLScriptEdCore::onMonoCheckboxClicked(LLUICtrl*, void* userdata)
{
	LLScriptEdCore* self = static_cast<LLScriptEdCore*>(userdata);
	self->enableSave(TRUE);
}

BOOL LLScriptEdCore::monoChecked() const
{
	return mMonoCheckbox && mMonoCheckbox->getValue() ? TRUE : FALSE;
}

/// ---------------------------------------------------------------------------
/// LLPreviewLSL
/// ---------------------------------------------------------------------------

struct LLScriptSaveInfo
{
	LLUUID mItemUUID;
	std::string mDescription;
	LLTransactionID mTransactionID;

	LLScriptSaveInfo(const LLUUID& uuid, const std::string& desc, LLTransactionID tid) :
		mItemUUID(uuid), mDescription(desc),  mTransactionID(tid) {}
};

//static
void* LLPreviewLSL::createScriptEdPanel(void* userdata)
{
	LLPreviewLSL *self = (LLPreviewLSL*)userdata;

	self->mScriptEd =  new LLScriptEdCore("script panel", LLRect(),
										  HELLO_LSL, LSL_DOC_URL,
										  self->getHandle(),
										  LLPreviewLSL::onLoad,
										  LLPreviewLSL::onSave,
										  LLPreviewLSL::onSearchReplace,
										  self, 0);
	return self->mScriptEd;
}

LLPreviewLSL::LLPreviewLSL(const std::string& name, const LLRect& rect,
						   const std::string& title, const LLUUID& item_id)
:	LLPreview(name, rect, title, item_id, LLUUID::null, TRUE,
			  SCRIPT_MIN_WIDTH, SCRIPT_MIN_HEIGHT),
   mPendingUploads(0)
{
	LLRect curRect = rect;

	LLCallbackMap::map_t factory_map;
	factory_map["script panel"] = LLCallbackMap(LLPreviewLSL::createScriptEdPanel, this);

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_script_preview.xml", &factory_map);

	const LLInventoryItem* item = getItem();

	childSetCommitCallback("desc", LLPreview::onText, this);
	childSetText("desc", item->getDescription());
	childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);

	LLCheckBoxCtrl* monoCheckbox = mScriptEd->getMonoCheckBox();
	monoCheckbox->setEnabled(gIsInSecondLife && have_script_upload_cap(LLUUID::null));
	monoCheckbox->set(gSavedSettings.getBOOL("SaveInventoryScriptsAsMono"));

	if (!getFloaterHost() && !getHost() && getAssetStatus() == PREVIEW_ASSET_UNLOADED)
	{
		loadAsset();
	}

	setTitle(title);
	mScriptEd->setScriptName(title);

	if (!getHost())
	{
		reshape(curRect.getWidth(), curRect.getHeight(), TRUE);
		setRect(curRect);
	}
}

// virtual
void LLPreviewLSL::callbackLSLCompileSucceeded()
{
	llinfos << "LSL Bytecode saved" << llendl;
	mScriptEd->mErrorList->addCommentText(getString("compile_success"));
	mScriptEd->mErrorList->addCommentText(getString("save_complete"));
	closeIfNeeded();
}

// virtual
void LLPreviewLSL::callbackLSLCompileFailed(const LLSD& compile_errors)
{
	llinfos << "Compile failed!" << llendl;

	for (LLSD::array_const_iterator line = compile_errors.beginArray();
		line < compile_errors.endArray();
		line++)
	{
		LLSD row;
		std::string error_message = line->asString();
		LLStringUtil::stripNonprintable(error_message);
		row["columns"][0]["value"] = error_message;
		row["columns"][0]["font"] = "OCRA";
		mScriptEd->mErrorList->addElement(row);
	}
	mScriptEd->selectFirstError();
	closeIfNeeded();
}

void LLPreviewLSL::loadAsset()
{
	// *HACK: we poke into inventory to see if it's there, and if so,
	// then it might be part of the inventory library. If it's in the
	// library, then you can see the script, but not modify it.
	const LLInventoryItem* item = gInventory.getItem(mItemUUID);
	BOOL is_library = item
		&& !gInventory.isObjectDescendentOf(mItemUUID,
											gInventory.getRootFolderID());
	if (!item)
	{
		// do the more generic search.
		getItem();
	}
	if (item)
	{
		BOOL is_copyable = gAgent.allowOperation(PERM_COPY,
												 item->getPermissions(),
												 GP_OBJECT_MANIPULATE);
		BOOL is_modifiable = gAgent.allowOperation(PERM_MODIFY,
												   item->getPermissions(),
												   GP_OBJECT_MANIPULATE);
		if (gAgent.isGodlike() || (is_copyable && (is_modifiable || is_library)))
		{
			LLUUID* new_uuid = new LLUUID(mItemUUID);
			gAssetStorage->getInvItemAsset(LLHost::invalid,
										   gAgentID,
										   gAgent.getSessionID(),
										   item->getPermissions().getOwner(),
										   LLUUID::null,
										   item->getUUID(),
										   item->getAssetUUID(),
										   item->getType(),
										   &LLPreviewLSL::onLoadComplete,
										   (void*)new_uuid,
										   TRUE);
			mAssetStatus = PREVIEW_ASSET_LOADING;
		}
		else
		{
			mScriptEd->setScriptText(mScriptEd->getString("can_not_view"), FALSE);
			mScriptEd->mEditor->makePristine();
			mScriptEd->mEditor->setEnabled(FALSE);
			mScriptEd->mFunctions->setEnabled(FALSE);
			mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		childSetVisible("lock", !is_modifiable);
		mScriptEd->childSetEnabled("Insert...", is_modifiable);
	}
	else
	{
		mScriptEd->setScriptText(std::string(HELLO_LSL), TRUE);
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
}

BOOL LLPreviewLSL::canClose()
{
	return mScriptEd->canClose();
}

void LLPreviewLSL::closeIfNeeded()
{
	// Find our window and close it if requested.
	getWindow()->decBusyCount();
	mPendingUploads--;
	if (mPendingUploads <= 0 && mCloseAfterSave)
	{
		if (!mScriptEd->mAutosaveFilename.empty()) {
			llinfos << "remove autosave: " << mScriptEd->mAutosaveFilename << llendl;
			LLFile::remove(mScriptEd->mAutosaveFilename.c_str());
		}
		close();
	}
}

//override the llpreview open which attempts to load asset, load after xml ui made
void LLPreviewLSL::open()		/*Flawfinder: ignore*/
{
	LLFloater::open();		/*Flawfinder: ignore*/
}

void LLPreviewLSL::onSearchReplace(void* userdata)
{
	LLPreviewLSL* self = (LLPreviewLSL*)userdata;
	LLScriptEdCore* sec = self->mScriptEd; 
	if (sec && sec->mEditor)
	{
		LLFloaterSearchReplace::show(sec->mEditor);
	}
}

// static
void LLPreviewLSL::onLoad(void* userdata)
{
	LLPreviewLSL* self = (LLPreviewLSL*)userdata;
	self->loadAsset();
}

// static
void LLPreviewLSL::onSave(void* userdata, BOOL close_after_save)
{
	LLPreviewLSL* self = (LLPreviewLSL*)userdata;
	self->mCloseAfterSave = close_after_save;
	self->saveIfNeeded();
}

// Save needs to compile the text in the buffer. If the compile
// succeeds, then save both assets out to the database. If the compile
// fails, go ahead and save the text anyway so that the user doesn't
// get too fucked.
void LLPreviewLSL::saveIfNeeded()
{
	// llinfos << "LLPreviewLSL::saveIfNeeded()" << llendl;
	if (!LLScriptEdCore::hasChanged(mScriptEd))
	{
		return;
	}

	mPendingUploads = 0;
	mScriptEd->mErrorList->deleteAllItems();
	mScriptEd->mEditor->makePristine();
	mScriptEd->enableSave(FALSE);

	// save off asset into file
	LLTransactionID tid;
	tid.generate();
	LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, asset_id.asString());
	std::string filename = filepath + ".lsl";

	LLFILE* fp = LLFile::fopen(filename, "wb");
	if (!fp)
	{
		llwarns << "Unable to write to " << filename << llendl;

		LLSD row;
		row["columns"][0]["value"] = "Error writing to local file. Is your hard drive full?";
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		mScriptEd->mErrorList->addElement(row);
		return;
	}

	std::string utf8text = mScriptEd->mEditor->getText();
	fputs(utf8text.c_str(), fp);
	fclose(fp);
	fp = NULL;

	const LLInventoryItem *inv_item = getItem();
	// save it out to asset server
	std::string url = gAgent.getRegion()->getCapability("UpdateScriptAgent");
	if (inv_item)
	{
		getWindow()->incBusyCount();
		mPendingUploads++;
		if (!url.empty())
		{
			uploadAssetViaCaps(url, filename, mItemUUID);
		}
		else if (gAssetStorage)
		{
			uploadAssetLegacy(filename, mItemUUID, tid);
		}
	}
}

void LLPreviewLSL::uploadAssetViaCaps(const std::string& url,
									  const std::string& filename,
									  const LLUUID& item_id)
{
	llinfos << "Update Agent Inventory via capability" << llendl;
	LLSD body;
	body["item_id"] = item_id;
	body["target"] = gIsInSecondLife && mScriptEd->monoChecked() ? "mono" : "lsl2";
	LLHTTPClient::post(url, body, new LLUpdateAgentInventoryResponder(body,
																	  filename,
																	  LLAssetType::AT_LSL_TEXT));
	// Keep track of our last choice
	gSavedSettings.setBOOL("SaveInventoryScriptsAsMono", mScriptEd->monoChecked());
}

void LLPreviewLSL::uploadAssetLegacy(const std::string& filename,
									  const LLUUID& item_id,
									  const LLTransactionID& tid)
{
	LLLineEditor* descEditor = getChild<LLLineEditor>("desc");
	LLScriptSaveInfo* info = new LLScriptSaveInfo(item_id,
												  descEditor->getText(),
												  tid);
	gAssetStorage->storeAssetData(filename,	tid,
								  LLAssetType::AT_LSL_TEXT,
								  &LLPreviewLSL::onSaveComplete,
								  info);

	LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														  asset_id.asString());
	std::string dst_filename = llformat("%s.lso", filepath.c_str());
	std::string err_filename = llformat("%s.out", filepath.c_str());

	const BOOL compile_to_mono = FALSE;
	if (!lscript_compile(filename.c_str(),
						dst_filename.c_str(),
						err_filename.c_str(),
						compile_to_mono,
						asset_id.asString().c_str(),
						gAgent.isGodlike()))
	{
		llinfos << "Compile failed!" << llendl;
		//char command[256];
		//sprintf(command, "type %s\n", err_filename.c_str());
		//system(command);

		// load the error file into the error scrolllist
		LLFILE* fp = LLFile::fopen(err_filename, "r");
		if (fp)
		{
			char buffer[MAX_STRING];		/*Flawfinder: ignore*/
			std::string line;
			while (!feof(fp)) 
			{
				if (fgets(buffer, MAX_STRING, fp) == NULL)
				{
					buffer[0] = '\0';
				}
				if (feof(fp))
				{
					break;
				}
				else
				{
					line.assign(buffer);
					LLStringUtil::stripNonprintable(line);

					LLSD row;
					row["columns"][0]["value"] = line;
					row["columns"][0]["font"] = "OCRA";
					mScriptEd->mErrorList->addElement(row);
				}
			}
			fclose(fp);
			mScriptEd->selectFirstError();
		}
	}
	else
	{
		llinfos << "Compile worked!" << llendl;
		if (gAssetStorage)
		{
			getWindow()->incBusyCount();
			mPendingUploads++;
			LLUUID* this_uuid = new LLUUID(mItemUUID);
			gAssetStorage->storeAssetData(dst_filename,
										  tid,
										  LLAssetType::AT_LSL_BYTECODE,
										  &LLPreviewLSL::onSaveBytecodeComplete,
										  (void**)this_uuid);
		}
	}

	// get rid of any temp files left lying around
	LLFile::remove(filename);
	LLFile::remove(err_filename);
	LLFile::remove(dst_filename);
}

// static
void LLPreviewLSL::onSaveComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLScriptSaveInfo* info = reinterpret_cast<LLScriptSaveInfo*>(user_data);
	if (0 == status)
	{
		if (info)
		{
			const LLViewerInventoryItem* item;
			item = (const LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
			if (item)
			{
				LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				new_item->setAssetUUID(asset_uuid);
				new_item->setTransactionID(info->mTransactionID);
				new_item->updateServer(FALSE);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
			}
			else
			{
				llwarns << "Inventory item for script " << info->mItemUUID
					<< " is no longer in agent inventory." << llendl;
			}

			// Find our window and close it if requested.
			LLPreviewLSL* self = (LLPreviewLSL*)LLPreview::find(info->mItemUUID);
			if (self)
			{
				getWindow()->decBusyCount();
				self->mPendingUploads--;
				if (self->mPendingUploads <= 0
					&& self->mCloseAfterSave)
				{
					self->close();
				}
			}
		}
	}
	else
	{
		llwarns << "Problem saving script: " << status << llendl;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotifications::instance().add("SaveScriptFailReason", args);
	}
	delete info;
}

// static
void LLPreviewLSL::onSaveBytecodeComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLUUID* instance_uuid = (LLUUID*)user_data;
	LLPreviewLSL* self = NULL;
	if (instance_uuid)
	{
		self = LLPreviewLSL::getInstance(*instance_uuid);
	}
	if (0 == status)
	{
		if (self)
		{
			LLSD row;
			row["columns"][0]["value"] = "Compile successful!";
			row["columns"][0]["font"] = "SANSSERIF_SMALL";
			self->mScriptEd->mErrorList->addElement(row);

			// Find our window and close it if requested.
			self->getWindow()->decBusyCount();
			self->mPendingUploads--;
			if (self->mPendingUploads <= 0
				&& self->mCloseAfterSave)
			{
				self->close();
			}
		}
	}
	else
	{
		llwarns << "Problem saving LSL Bytecode (Preview)" << llendl;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotifications::instance().add("SaveBytecodeFailReason", args);
	}
	delete instance_uuid;
}

// static
void LLPreviewLSL::onLoadComplete(LLVFS *vfs, const LLUUID& asset_uuid, LLAssetType::EType type,
								   void* user_data, S32 status, LLExtStat ext_status)
{
	LL_DEBUGS("ScriptEditor") << "LLPreviewLSL::onLoadComplete: got uuid "
							  << asset_uuid << LL_ENDL;
	LLUUID* item_uuid = (LLUUID*)user_data;
	LLPreviewLSL* preview = LLPreviewLSL::getInstance(*item_uuid);
	if (preview)
	{
		if (0 == status)
		{
			LLVFile file(vfs, asset_uuid, type);
			S32 file_length = file.getSize();

			char* buffer = new char[file_length+1];
			file.read((U8*)buffer, file_length);		/*Flawfinder: ignore*/

			// put a EOS at the end
			buffer[file_length] = 0;
			preview->mScriptEd->setScriptText(LLStringExplicit(&buffer[0]), TRUE);
			preview->mScriptEd->mEditor->makePristine();
			delete [] buffer;
			LLInventoryItem* item = gInventory.getItem(*item_uuid);
			BOOL is_modifiable = FALSE;
			if (item
			   && gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
				   					GP_OBJECT_MANIPULATE))
			{
				is_modifiable = TRUE;
			}
			preview->mScriptEd->mEditor->setEnabled(is_modifiable);
			preview->mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else
		{
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

			if (LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
				LL_ERR_FILE_EMPTY == status)
			{
				LLNotifications::instance().add("ScriptMissing");
			}
			else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
			{
				LLNotifications::instance().add("ScriptNoPermissions");
			}
			else
			{
				LLNotifications::instance().add("UnableToLoadScript");
			}

			preview->mAssetStatus = PREVIEW_ASSET_ERROR;
			llwarns << "Problem loading script: " << status << llendl;
		}
	}
	delete item_uuid;
}

// static
LLPreviewLSL* LLPreviewLSL::getInstance(const LLUUID& item_uuid)
{
	LLPreview* instance = NULL;
	preview_map_t::iterator found_it = LLPreview::sInstances.find(item_uuid);
	if (found_it != LLPreview::sInstances.end())
	{
		instance = found_it->second;
	}
	return (LLPreviewLSL*)instance;
}

void LLPreviewLSL::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPreview::reshape(width, height, called_from_parent);

	if (!isMinimized())
	{
		// So that next time you open a script it will have the same height and width 
		// (although not the same position).
		gSavedSettings.setRect("PreviewScriptRect", getRect());
	}
}

/// ---------------------------------------------------------------------------
/// LLLiveLSLEditor
/// ---------------------------------------------------------------------------

LLMap<LLUUID, LLLiveLSLEditor*> LLLiveLSLEditor::sInstances;

//static 
void* LLLiveLSLEditor::createScriptEdPanel(void* userdata)
{

	LLLiveLSLEditor *self = (LLLiveLSLEditor*)userdata;

	self->mScriptEd =  new LLScriptEdCore("script ed panel", LLRect(),
										  HELLO_LSL, LSL_DOC_URL,
										  self->getHandle(),
										  &LLLiveLSLEditor::onLoad,
										  &LLLiveLSLEditor::onSave,
										  &LLLiveLSLEditor::onSearchReplace,
										  self, 0);

	return self->mScriptEd;
}

LLLiveLSLEditor::LLLiveLSLEditor(const std::string& name,
								 const LLRect& rect,
								 const std::string& title,
								 const LLUUID& object_id,
								 const LLUUID& item_id)
:	LLPreview(name, rect, title, item_id, object_id, TRUE, SCRIPT_MIN_WIDTH, SCRIPT_MIN_HEIGHT),
	mObjectID(object_id),
	mItemID(item_id),
	mScriptEd(NULL),
	mAskedForRunningInfo(FALSE),
	mHaveRunningInfo(FALSE),
	mCloseAfterSave(FALSE),
	mPendingUploads(0),
	mIsModifiable(FALSE)
{
	BOOL is_new = FALSE;
	if (mItemID.isNull())
	{
		mItemID.generate();
		is_new = TRUE;
	}

	LLLiveLSLEditor::sInstances.addData(mItemID ^ mObjectID, this);

	LLCallbackMap::map_t factory_map;
	factory_map["script ed panel"] = LLCallbackMap(LLLiveLSLEditor::createScriptEdPanel,
												   this);

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_live_lsleditor.xml",
												 &factory_map);

	mRunningCheckbox = getChild<LLCheckBoxCtrl>("running");
	mRunningCheckbox->setCommitCallback(onRunningCheckboxClicked);
	mRunningCheckbox->setCallbackUserData(this);
	mRunningCheckbox->setEnabled(FALSE);

	childSetAction("Reset", &LLLiveLSLEditor::onReset, this);
	childSetEnabled("Reset", true);

	mScriptEd->mEditor->makePristine();
	loadAsset(is_new);
	mScriptEd->mEditor->setFocus(true);

	if (!getHost())
	{
		LLRect curRect = getRect();
		translate(rect.mLeft - curRect.mLeft, rect.mTop - curRect.mTop);
	}

	setTitle(title);
	mScriptEd->setScriptName(title);

	mScriptRunningText = getString("script_running");
	mCannotRunText = getString("public_objects_can_not_run");
	mOutOfRange = getString("out_of_range");
}

LLLiveLSLEditor::~LLLiveLSLEditor()
{
	LLLiveLSLEditor::sInstances.removeData(mItemID ^ mObjectID);
}

// this is called via LLPreview::loadAsset() virtual method
void LLLiveLSLEditor::loadAsset()
{
	loadAsset(FALSE);
}

// virtual
void LLLiveLSLEditor::callbackLSLCompileSucceeded(const LLUUID& task_id,
												  const LLUUID& item_id,
												  bool is_script_running)
{
	LL_DEBUGS("ScriptEditor") << "LSL Bytecode saved" << LL_ENDL;
	mScriptEd->mErrorList->addCommentText(getString("compile_success"));
	mScriptEd->mErrorList->addCommentText(getString("save_complete"));
	closeIfNeeded();
}

// virtual
void LLLiveLSLEditor::callbackLSLCompileFailed(const LLSD& compile_errors)
{
	LL_DEBUGS("ScriptEditor") << "Compile failed !" << LL_ENDL;
	for (LLSD::array_const_iterator line = compile_errors.beginArray();
		 line < compile_errors.endArray(); ++line)
	{
		LLSD row;
		std::string error_message = line->asString();
		LLStringUtil::stripNonprintable(error_message);
		row["columns"][0]["value"] = error_message;
		row["columns"][0]["font"] = "OCRA";
		mScriptEd->mErrorList->addElement(row);
	}
	mScriptEd->selectFirstError();
	closeIfNeeded();
}

void LLLiveLSLEditor::loadAsset(BOOL is_new)
{
	//llinfos << "LLLiveLSLEditor::loadAsset()" << llendl;
	if (!is_new)
	{
		LLViewerObject* object = gObjectList.findObject(mObjectID);
		if (object)
		{
			// HACK !  We "know" that mItemID refers to a LLViewerInventoryItem
			LLViewerInventoryItem* item;
			item = (LLViewerInventoryItem*)object->getInventoryObject(mItemID);
			if (item &&
				(gAgent.allowOperation(PERM_COPY, item->getPermissions(),
									   GP_OBJECT_MANIPULATE) ||
				 gAgent.isGodlike()))
			{
				mItem = new LLViewerInventoryItem(item);
				//llinfos << "asset id " << mItem->getAssetUUID() << llendl;
			}

			if (!gAgent.isGodlike() &&
				(item &&
				 (!gAgent.allowOperation(PERM_COPY, item->getPermissions(),
										 GP_OBJECT_MANIPULATE) ||
				  !gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
										 GP_OBJECT_MANIPULATE))))
			{
				mItem = new LLViewerInventoryItem(item);
				mScriptEd->setScriptText(getString("not_allowed"), FALSE);
				mScriptEd->mEditor->makePristine();
				mScriptEd->mEditor->setEnabled(FALSE);
				mScriptEd->enableSave(FALSE);
				mAssetStatus = PREVIEW_ASSET_LOADED;
			}
			else if (item && mItem.notNull())
			{
				// request the text from the object
				LLUUID* user_data = new LLUUID(mItemID ^ mObjectID);
				gAssetStorage->getInvItemAsset(object->getRegion()->getHost(),
											   gAgentID, gAgent.getSessionID(),
											   item->getPermissions().getOwner(),
											   object->getID(),
											   item->getUUID(),
											   item->getAssetUUID(),
											   item->getType(),
											   &LLLiveLSLEditor::onLoadComplete,
											   (void*)user_data, TRUE);
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_GetScriptRunning);
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, mObjectID);
				msg->addUUIDFast(_PREHASH_ItemID, mItemID);
				msg->sendReliable(object->getRegion()->getHost());
				mAskedForRunningInfo = TRUE;
				mAssetStatus = PREVIEW_ASSET_LOADING;
			}
			else
			{
				mScriptEd->setScriptText(LLStringUtil::null, FALSE);
				mScriptEd->mEditor->makePristine();
				mAssetStatus = PREVIEW_ASSET_LOADED;
			}

			mIsModifiable = item && gAgent.allowOperation(PERM_MODIFY, 
										item->getPermissions(),
				   						GP_OBJECT_MANIPULATE);
			if (!mIsModifiable)
			{
				mScriptEd->mEditor->setEnabled(FALSE);
			}

			// This is commented out, because we don't completely
			// handle script exports yet.
			/*
			// request the exports from the object
			gMessageSystem->newMessage("GetScriptExports");
			gMessageSystem->nextBlock("ScriptBlock");
			gMessageSystem->addUUID("AgentID", gAgentID);
			U32 local_id = object->getLocalID();
			gMessageSystem->addData("LocalID", &local_id);
			gMessageSystem->addUUID("ItemID", mItemID);
			LLHost host(object->getRegion()->getIP(),
						object->getRegion()->getPort());
			gMessageSystem->sendReliable(host);
			*/
		}

		// Initialization of the asset failed. Probably the result 
		// of a bug somewhere else. Set up this editor in a no-go mode.
		if (mItem.isNull())
		{
			// Set the inventory item to an incomplete item.
			// This may be better than having a accessible null pointer around,
			// though this newly allocated object will most likely be replaced.
			mItem = new LLViewerInventoryItem();
			mScriptEd->setScriptText(LLStringUtil::null, FALSE);
			mScriptEd->mEditor->makePristine();
			mScriptEd->mEditor->setEnabled(FALSE);
			mAssetStatus = PREVIEW_ASSET_LOADED;
		}
	}
	else
	{
		mScriptEd->setScriptText(std::string(HELLO_LSL), TRUE);
		mScriptEd->enableSave(FALSE);
		LLPermissions perm;
		perm.init(gAgentID, gAgentID, LLUUID::null, gAgent.getGroupID());
		perm.initMasks(PERM_ALL, PERM_ALL, PERM_NONE, PERM_NONE, PERM_MOVE | PERM_TRANSFER);
		mItem = new LLViewerInventoryItem(mItemID,
									mObjectID,
									perm,
									LLUUID::null,
									LLAssetType::AT_LSL_TEXT,
									LLInventoryType::IT_LSL,
									DEFAULT_SCRIPT_NAME,
									DEFAULT_SCRIPT_DESC,
									LLSaleInfo::DEFAULT,
									LLInventoryItem::II_FLAGS_NONE,
									time_corrected());
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
}

// static
void LLLiveLSLEditor::onLoadComplete(LLVFS *vfs, const LLUUID& asset_id,
									 LLAssetType::EType type,
									 void* user_data, S32 status, LLExtStat ext_status)
{
	LL_DEBUGS("ScriptEditor") << "LLLiveLSLEditor::onLoadComplete: got uuid "
							  << asset_id << LL_ENDL;
	LLLiveLSLEditor* instance = NULL;
	LLUUID* xored_id = (LLUUID*)user_data;

	if (LLLiveLSLEditor::sInstances.checkData(*xored_id))
	{
		instance = LLLiveLSLEditor::sInstances[*xored_id];
		if (LL_ERR_NOERR == status)
		{
			instance->loadScriptText(vfs, asset_id, type);
			instance->mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else
		{
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

			if (LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
				LL_ERR_FILE_EMPTY == status)
			{
				LLNotifications::instance().add("ScriptMissing");
			}
			else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
			{
				LLNotifications::instance().add("ScriptNoPermissions");
			}
			else
			{
				LLNotifications::instance().add("UnableToLoadScript");
			}
			instance->mAssetStatus = PREVIEW_ASSET_ERROR;
		}
	}

	delete xored_id;
}

void LLLiveLSLEditor::loadScriptText(LLVFS *vfs, const LLUUID &uuid,
									 LLAssetType::EType type)
{
	LLVFile file(vfs, uuid, type);
	S32 file_length = file.getSize();
	char *buffer = new char[file_length + 1];
	file.read((U8*)buffer, file_length);		/*Flawfinder: ignore*/

	if (file.getLastBytesRead() != file_length ||
		file_length <= 0)
	{
		llwarns << "Error reading " << uuid << ":" << type << llendl;
	}

	buffer[file_length] = '\0';

	mScriptEd->setScriptText(LLStringExplicit(&buffer[0]), TRUE);
	mScriptEd->mEditor->makePristine();
	delete[] buffer;
}

void LLLiveLSLEditor::onRunningCheckboxClicked(LLUICtrl*, void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if (!self) return;

	LLViewerObject* object = gObjectList.findObject(self->mObjectID);
	BOOL running = self->mRunningCheckbox->get();
//MK
	if (gRRenabled && !gAgent.mRRInterface.canDetach(object))
	{
		self->mRunningCheckbox->set(!running);
		return;
	}
//mk
	if (object)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_SetScriptRunning);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_Script);
		msg->addUUIDFast(_PREHASH_ObjectID, self->mObjectID);
		msg->addUUIDFast(_PREHASH_ItemID, self->mItemID);
		msg->addBOOLFast(_PREHASH_Running, running);
		msg->sendReliable(object->getRegion()->getHost());
	}
	else
	{
		self->mRunningCheckbox->set(!running);
		LLNotifications::instance().add("CouldNotStartStopScript");
	}
}

void LLLiveLSLEditor::onReset(void *userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*) userdata;

	LLViewerObject* object = gObjectList.findObject(self->mObjectID);
//MK
	if (gRRenabled && !gAgent.mRRInterface.canDetach(object))
	{
		return;
	}
//mk
	if (object)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_ScriptReset);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_Script);
		msg->addUUIDFast(_PREHASH_ObjectID, self->mObjectID);
		msg->addUUIDFast(_PREHASH_ItemID, self->mItemID);
		msg->sendReliable(object->getRegion()->getHost());
	}
	else
	{
		LLNotifications::instance().add("CouldNotStartStopScript"); 
	}
}

void LLLiveLSLEditor::draw()
{
	LLViewerObject* object = gObjectList.findObject(mObjectID);
	if (object && mAskedForRunningInfo && mHaveRunningInfo)
	{
		if (object->permAnyOwner())
		{
			mRunningCheckbox->setLabel(mScriptRunningText);
			mRunningCheckbox->setEnabled(TRUE);

			if (object->permAnyOwner())
			{
				mRunningCheckbox->setLabel(mScriptRunningText);
				mRunningCheckbox->setEnabled(TRUE);
			}
			else
			{
				mRunningCheckbox->setLabel(mCannotRunText);
				mRunningCheckbox->setEnabled(FALSE);
				// *FIX: Set it to false so that the ui is correct for
				// a box that is released to public. It could be
				// incorrect after a release/claim cycle, but will be
				// correct after clicking on it.
				mRunningCheckbox->set(FALSE);
				if (mScriptEd)
				{
					mScriptEd->getMonoCheckBox()->set(FALSE);
				}
			}
		}
		else
		{
			mRunningCheckbox->setLabel(mCannotRunText);
			mRunningCheckbox->setEnabled(FALSE);

			// *FIX: Set it to false so that the ui is correct for a box that
			// is released to public. It could be incorrect after a release/
			// claim cycle, but will be correct after clicking on it.
			mRunningCheckbox->set(FALSE);
			if (mScriptEd)
			{
				mScriptEd->getMonoCheckBox()->setEnabled(FALSE);
			}
			// object may have fallen out of range.
			mHaveRunningInfo = FALSE;
		}
	}
	else if (!object)
	{
		setTitle(mOutOfRange);
		mRunningCheckbox->setEnabled(FALSE);
		// object may have fallen out of range.
		mHaveRunningInfo = FALSE;
	}

	LLFloater::draw();
}

void LLLiveLSLEditor::onSearchReplace(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;

	LLScriptEdCore* sec = self->mScriptEd; 
	if (sec && sec->mEditor)
	{
		LLFloaterSearchReplace::show(sec->mEditor);
	}
}

struct LLLiveLSLSaveData
{
	LLLiveLSLSaveData(const LLUUID& id, const LLViewerInventoryItem* item,
					  BOOL active)
	:	mObjectID(id),
		mActive(active)

	{
		llassert(item);
		mItem = new LLViewerInventoryItem(item);
	}

	LLUUID								mObjectID;
	BOOL								mActive;
	LLPointer<LLViewerInventoryItem>	mItem;
};

void LLLiveLSLEditor::saveIfNeeded()
{
	llinfos << "LLLiveLSLEditor::saveIfNeeded()" << llendl;
	LLViewerObject* object = gObjectList.findObject(mObjectID);
	if (!object)
	{
		LLNotifications::instance().add("SaveScriptFailObjectNotFound");
		return;
	}

	if (mItem.isNull() || !mItem->isFinished())
	{
		// $NOTE: While the error message may not be exactly correct,
		// it's pretty close.
		LLNotifications::instance().add("SaveScriptFailObjectNotFound");
		return;
	}

	// get the latest info about it. We used to be losing the script
	// name on save, because the viewer object version of the item,
	// and the editor version would get out of synch. Here's a good
	// place to synch them back up.
	// HACK! we "know" that mItemID refers to a LLInventoryItem...
	LLInventoryItem* inv_item = (LLInventoryItem*)object->getInventoryObject(mItemID);
	if (inv_item)
	{
		mItem->copyItem(inv_item);
	}

	// Don't need to save if we're pristine
	if (!LLScriptEdCore::hasChanged(mScriptEd))
	{
		return;
	}

	mPendingUploads = 0;

	// save the script
	mScriptEd->enableSave(FALSE);
	mScriptEd->mEditor->makePristine();
	mScriptEd->mErrorList->deleteAllItems();

	// set up the save on the local machine.
	mScriptEd->mEditor->makePristine();
	LLTransactionID tid;
	tid.generate();
	LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,asset_id.asString());
	std::string filename = llformat("%s.lsl", filepath.c_str());

	mItem->setAssetUUID(asset_id);
	mItem->setTransactionID(tid);

	// write out the data, and store it in the asset database
	LLFILE* fp = LLFile::fopen(filename, "wb");
	if (!fp)
	{
		llwarns << "Unable to write to " << filename << llendl;

		LLSD row;
		row["columns"][0]["value"] = "Error writing to local file. Is your hard drive full?";
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		mScriptEd->mErrorList->addElement(row);
		return;
	}
	std::string utf8text = mScriptEd->mEditor->getText();

	// Special case for a completely empty script - stuff in one space so it can store properly.  See SL-46889
	if (utf8text.size() == 0)
	{
		utf8text = " ";
	}

	fputs(utf8text.c_str(), fp);
	fclose(fp);
	fp = NULL;

	// save it out to asset server
	std::string url = object->getRegion()->getCapability("UpdateScriptTask");
	getWindow()->incBusyCount();
	mPendingUploads++;
	BOOL is_running = getChild<LLCheckBoxCtrl>("running")->get();
	if (!url.empty())
	{
		uploadAssetViaCaps(url, filename, mObjectID,
						   mItemID, is_running);
	}
	else if (gAssetStorage)
	{
		uploadAssetLegacy(filename, object, tid, is_running);
	}
}

void LLLiveLSLEditor::uploadAssetViaCaps(const std::string& url,
										 const std::string& filename,
										 const LLUUID& task_id,
										 const LLUUID& item_id,
										 BOOL is_running)
{
	llinfos << "Update Task Inventory via capability" << llendl;
	LLSD body;
	body["task_id"] = task_id;
	body["item_id"] = item_id;
	body["is_script_running"] = is_running;
	body["target"] = gIsInSecondLife && mScriptEd->monoChecked() ? "mono" : "lsl2";
	LLHTTPClient::post(url, body, new LLUpdateTaskInventoryResponder(body,
																	 filename,
																	 LLAssetType::AT_LSL_TEXT));
}

void LLLiveLSLEditor::uploadAssetLegacy(const std::string& filename,
										LLViewerObject* object,
										const LLTransactionID& tid,
										BOOL is_running)
{
	LLLiveLSLSaveData* data = new LLLiveLSLSaveData(mObjectID, mItem,
													is_running);
	gAssetStorage->storeAssetData(filename, tid, LLAssetType::AT_LSL_TEXT,
								  &onSaveTextComplete, (void*)data, FALSE);

	LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														  asset_id.asString());
	std::string dst_filename = llformat("%s.lso", filepath.c_str());
	std::string err_filename = llformat("%s.out", filepath.c_str());

	LLFILE *fp;
	const BOOL compile_to_mono = FALSE;
	if (!lscript_compile(filename.c_str(), dst_filename.c_str(),
						 err_filename.c_str(), compile_to_mono,
						 asset_id.asString().c_str(), gAgent.isGodlike()))
	{
		// load the error file into the error scrolllist
		llinfos << "Compile failed!" << llendl;
		if (NULL != (fp = LLFile::fopen(err_filename, "r")))
		{
			char buffer[MAX_STRING];		/*Flawfinder: ignore*/
			std::string line;
			while (!feof(fp)) 
			{

				if (fgets(buffer, MAX_STRING, fp) == NULL)
				{
					buffer[0] = '\0';
				}
				if (feof(fp))
				{
					break;
				}
				else
				{
					line.assign(buffer);
					LLStringUtil::stripNonprintable(line);

					LLSD row;
					row["columns"][0]["value"] = line;
					row["columns"][0]["font"] = "OCRA";
					mScriptEd->mErrorList->addElement(row);
				}
			}
			fclose(fp);
			mScriptEd->selectFirstError();
			// don't set the asset id, because we want to save the
			// script, even though the compile failed.
			//mItem->setAssetUUID(LLUUID::null);
			object->saveScript(mItem, FALSE, false);
			dialog_refresh_all();
		}
	}
	else
	{
		llinfos << "Compile successful !" << llendl;
		mScriptEd->mErrorList->addCommentText(getString("success_saving"));
		if (gAssetStorage)
		{
			llinfos << "LLLiveLSLEditor::saveAsset "
					<< mItem->getAssetUUID() << llendl;
			getWindow()->incBusyCount();
			mPendingUploads++;
			LLLiveLSLSaveData* data = NULL;
			data = new LLLiveLSLSaveData(mObjectID,
										 mItem,
										 is_running);
			gAssetStorage->storeAssetData(dst_filename,
										  tid,
										  LLAssetType::AT_LSL_BYTECODE,
										  &LLLiveLSLEditor::onSaveBytecodeComplete,
										  (void*)data);
			dialog_refresh_all();
		}
	}

	// get rid of any temp files left lying around
	LLFile::remove(filename);
	LLFile::remove(err_filename);
	LLFile::remove(dst_filename);

	// If we successfully saved it, then we should be able to check/uncheck
	// the running box!
	mRunningCheckbox->setLabel(mScriptRunningText);
	mRunningCheckbox->setEnabled(TRUE);
}

// StoreAssetData callback (fixed)
void LLLiveLSLEditor::onSaveTextComplete(const LLUUID& asset_uuid,
										 void* user_data, S32 status,
										 LLExtStat ext_status)
{
	LLLiveLSLSaveData* data = (LLLiveLSLSaveData*)user_data;

	if (status)
	{
		llwarns << "Unable to save text for a script." << llendl;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotifications::instance().add("CompileQueueSaveText", args);
	}
	else
	{
		LLLiveLSLEditor* self;
		self = sInstances.getIfThere(data->mItem->getUUID() ^ data->mObjectID);
		if (self)
		{
			self->getWindow()->decBusyCount();
			if (--self->mPendingUploads <= 0 && self->mCloseAfterSave)
			{
				self->close();
			}
		}
	}
	delete data;
	data = NULL;
}

void LLLiveLSLEditor::onSaveBytecodeComplete(const LLUUID& asset_uuid,
											 void* user_data,
											 S32 status,
											 LLExtStat ext_status)
{
	LLLiveLSLSaveData* data = (LLLiveLSLSaveData*)user_data;
	if (!data) return;
	if (status == 0)
	{
		llinfos << "LSL Bytecode saved" << llendl;
		LLUUID xor_id = data->mItem->getUUID() ^ data->mObjectID;
		LLLiveLSLEditor* self = sInstances.getIfThere(xor_id);
		if (self)
		{
			// Tell the user that the compile worked.
			self->mScriptEd->mErrorList->addCommentText(self->getString("save_complete"));
			// close the window if this completes both uploads
			self->getWindow()->decBusyCount();
			self->mPendingUploads--;
			if (self->mPendingUploads <= 0
				&& self->mCloseAfterSave)
			{
				self->close();
			}
		}
		LLViewerObject* object = gObjectList.findObject(data->mObjectID);
		if (object)
		{
			object->saveScript(data->mItem, data->mActive, false);
			dialog_refresh_all();
			//LLToolDragAndDrop::dropScript(object, ids->first,
			//						  LLAssetType::AT_LSL_TEXT, FALSE);
		}
	}
	else
	{
		llinfos << "Problem saving LSL Bytecode (Live Editor)" << llendl;
		llwarns << "Unable to save a compiled script." << llendl;

		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotifications::instance().add("CompileQueueSaveBytecode", args);
	}

	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,asset_uuid.asString());
	std::string dst_filename = llformat("%s.lso", filepath.c_str());
	LLFile::remove(dst_filename);
	delete data;
}

void LLLiveLSLEditor::open()
{
	LLFloater::open();		/*Flawfinder: ignore*/
}

BOOL LLLiveLSLEditor::canClose()
{
	return (mScriptEd->canClose());
}

void LLLiveLSLEditor::closeIfNeeded()
{
	getWindow()->decBusyCount();
	if (--mPendingUploads <= 0 && mCloseAfterSave)
	{
		if (!mScriptEd->mAutosaveFilename.empty())
		{
			llinfos << "remove autosave: " << mScriptEd->mAutosaveFilename << llendl;
			LLFile::remove(mScriptEd->mAutosaveFilename.c_str());
		}
		close();
	}
}

// static
void LLLiveLSLEditor::onLoad(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	self->loadAsset();
}

// static
void LLLiveLSLEditor::onSave(void* userdata, BOOL close_after_save)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*) userdata;
//MK
	if (gRRenabled)
	{
		LLViewerObject* object = gObjectList.findObject(self->mObjectID);
		if (!gAgent.mRRInterface.canDetach(object))
		{
			return;
		}
	}
// mk
	self->mCloseAfterSave = close_after_save;
	self->saveIfNeeded();
}

// static
LLLiveLSLEditor* LLLiveLSLEditor::show(const LLUUID& script_id, const LLUUID& object_id)
{
	LLLiveLSLEditor* instance = NULL;
	LLUUID xored_id = script_id ^ object_id;
	if (LLLiveLSLEditor::sInstances.checkData(xored_id))
	{
		// Move the existing view to the front
		instance = LLLiveLSLEditor::sInstances[xored_id];
		instance->open();		/*Flawfinder: ignore*/
	}
	return instance;
}

// static
void LLLiveLSLEditor::hide(const LLUUID& script_id, const LLUUID& object_id)
{
	LLUUID xored_id = script_id ^ object_id;
	if (LLLiveLSLEditor::sInstances.checkData(xored_id))
	{
		LLLiveLSLEditor* instance = LLLiveLSLEditor::sInstances[xored_id];
		if (instance->getParent())
		{
			instance->getParent()->removeChild(instance);
		}
		delete instance;
	}
}
// static
LLLiveLSLEditor* LLLiveLSLEditor::find(const LLUUID& script_id, const LLUUID& object_id)
{
	LLUUID xored_id = script_id ^ object_id;
	return sInstances.getIfThere(xored_id);
}

// static
void LLLiveLSLEditor::processScriptRunningReply(LLMessageSystem* msg, void**)
{
	LLUUID item_id;
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_Script, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_Script, _PREHASH_ItemID, item_id);
	LLUUID xored_id = item_id ^ object_id;
	if (LLLiveLSLEditor::sInstances.checkData(xored_id))
	{
		LLLiveLSLEditor* instance = LLLiveLSLEditor::sInstances[xored_id];
		instance->mHaveRunningInfo = TRUE;
		BOOL running;
		msg->getBOOLFast(_PREHASH_Script, _PREHASH_Running, running);
		instance->mRunningCheckbox->set(running);
		BOOL mono;
		msg->getBOOLFast(_PREHASH_Script, "Mono", mono);
		LLCheckBoxCtrl* monoCheckbox = instance->mScriptEd->getMonoCheckBox();
		monoCheckbox->setEnabled(gIsInSecondLife && instance->getIsModifiable() &&
								 have_script_upload_cap(object_id));
		monoCheckbox->set(mono);
	}
}

void LLLiveLSLEditor::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLFloater::reshape(width, height, called_from_parent);

	if (!isMinimized())
	{
		// So that next time you open a script it will have the same height and width 
		// (although not the same position).
		gSavedSettings.setRect("PreviewScriptRect", getRect());
	}
}
