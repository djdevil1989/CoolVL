/**
 * @file llinventorybridge.h
 * @brief Implementation of the Inventory-Folder-View-Bridge classes.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLINVENTORYBRIDGE_H
#define LL_LLINVENTORYBRIDGE_H

#include "llcallingcard.h"
#include "llinventorymodel.h"
#include "llinventoryview.h"
#include "llfloaterproperties.h"
#include "llfolderview.h"
#include "llviewercontrol.h"
#include "llwearable.h"

// The "Restore to Last Position" feature is no more supported in SL and was
// never implemented in OpenSim...
#define RESTORE_TO_WORLD 0

typedef std::pair<LLUUID, LLUUID> two_uuids_t;
typedef std::list<two_uuids_t> two_uuids_list_t;
typedef std::pair<LLUUID, two_uuids_list_t> uuid_move_list_t;

struct LLMoveInv
{
	LLUUID mObjectID;
	LLUUID mCategoryID;
	two_uuids_list_t mMoveList;
	void (*mCallback)(S32, void*);
	void* mUserData;
};

struct LLAttachmentRezAction
{
	LLUUID	mItemID;
	S32		mAttachPt;
};

// Helper functions
class LLShowProps
{
public:

	static void showProperties(const LLUUID& uuid)
	{
		if (!LLFloaterProperties::show(uuid, LLUUID::null))
		{
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PropertiesRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);
			LLFloaterProperties* floater;
			floater = new LLFloaterProperties("item properties", rect,
											  "Inventory Item Properties",
											  uuid, LLUUID::null);
			// keep onscreen
			gFloaterView->adjustToFitScreen(floater, FALSE);
		}
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanelObserver
//
// Bridge to support knowing when the inventory has changed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryPanelObserver : public LLInventoryObserver
{
public:
	LLInventoryPanelObserver(LLInventoryPanel* ip)
	:	mIP(ip)
	{
	}

	virtual ~LLInventoryPanelObserver()	{}
	virtual void changed(U32 mask)		{ mIP->modelChanged(mask); }

protected:
	LLInventoryPanel* mIP;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvFVBridge (& it's derived classes)
//
// Short for Inventory-Folder-View-Bridge. This is an
// implementation class to be able to view inventory items.
//
// You'll want to call LLInvItemFVELister::createBridge() to actually create
// an instance of this class. This helps encapsulate the
// funcationality a bit. (except for folders, you can create those
// manually...)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInvFVBridge : public LLFolderViewEventListener
{
public:
	// This method is a convenience function which creates the correct
	// type of bridge based on some basic information
	static LLInvFVBridge* createBridge(LLAssetType::EType asset_type,
									   LLAssetType::EType actual_asset_type,
									   LLInventoryType::EType inv_type,
									   LLInventoryPanel* inventory,
									   const LLUUID& uuid,
									   U32 flags = 0x00);
	virtual ~LLInvFVBridge()				{}

	virtual const LLUUID& getUUID() const	{ return mUUID; }

	virtual const std::string& getPrefix()	{ return LLStringUtil::null; }
	virtual void restoreItem()				{}
#if RESTORE_TO_WORLD
	virtual void restoreToWorld()			{}
#endif

	// LLFolderViewEventListener functions
	virtual const std::string& getName() const;
	virtual const std::string& getDisplayName() const;
	virtual PermissionMask getPermissionMask() const;
	virtual LLFolderType::EType getPreferredType() const;
	virtual time_t getCreationDate() const;
	virtual LLFontGL::StyleFlags getLabelStyle() const		{ return LLFontGL::NORMAL; }
	virtual std::string getLabelSuffix() const				{ return LLStringUtil::null; }
	virtual void openItem()					{}
	virtual void previewItem()				{ openItem(); }
	virtual void showProperties();
	virtual BOOL isItemRenameable() const	{ return TRUE; }
	//virtual BOOL renameItem(const std::string& new_name)	{}
	virtual BOOL isItemRemovable();
	virtual BOOL isItemMovable();
	//virtual BOOL removeItem() = 0;
	virtual void removeBatch(LLDynamicArray<LLFolderViewEventListener*>& batch);
	virtual void move(LLFolderViewEventListener* new_parent_bridge) {}
	virtual BOOL isItemCopyable() const		{ return FALSE; }
	virtual BOOL copyToClipboard() const	{ return FALSE; }
	virtual BOOL cutToClipboard() const		{ return FALSE; }
	virtual BOOL isClipboardPasteable() const;
	virtual BOOL isClipboardPasteableAsLink() const;
	virtual void pasteFromClipboard()		{}
	virtual void pasteLinkFromClipboard()	{}
	void getClipboardEntries(bool show_asset_id, std::vector<std::string> &items,
		std::vector<std::string> &disabled_items, U32 flags);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual BOOL startDrag(EDragAndDropType* type, LLUUID* id) const;
	virtual BOOL dragOrDrop(MASK mask,
							BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data)	{ return FALSE; }
	virtual LLInventoryType::EType getInventoryType() const	{ return mInvType; }

	// LLInvFVBridge functionality
	virtual void clearDisplayName()			{}

//MK
////protected:
//mk
	LLInvFVBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	mInventoryPanel(inventory),
		mUUID(uuid),
		mInvType(LLInventoryType::IT_NONE)
	{}

	LLInventoryObject* getInventoryObject() const;
	BOOL isInTrash() const;
	BOOL isLinkedObjectInTrash() const; // Is this obj or its baseobj in the trash?
	BOOL isLinkedObjectMissing() const; // Is this a linked obj whose baseobj is not in inventory?
	// return true if the item is in agent inventory. if false, it
	// must be lost or in the inventory library.
	BOOL isAgentInventory() const;
	virtual BOOL isItemPermissive() const;
	static void changeItemParent(LLInventoryModel* model,
								 LLViewerInventoryItem* item,
								 const LLUUID& new_parent,
								 BOOL restamp);
	static void changeCategoryParent(LLInventoryModel* model,
									 LLViewerInventoryCategory* item,
									 const LLUUID& new_parent,
									 BOOL restamp);
	void removeBatchNoCheck(LLDynamicArray<LLFolderViewEventListener*>& batch);

protected:
	LLInventoryPanel* mInventoryPanel;
	LLUUID mUUID;	// item id
	LLInventoryType::EType mInvType;
};

class LLItemBridge : public LLInvFVBridge
{
public:
	LLItemBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLInvFVBridge(inventory, uuid)
	{}

	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);

	virtual void selectItem();
	virtual void restoreItem();
#if RESTORE_TO_WORLD
	virtual void restoreToWorld();
#endif
	virtual void gotoItem(LLFolderView *folder);
	virtual LLUIImagePtr getIcon() const;
	virtual const std::string& getDisplayName() const;
	virtual std::string getLabelSuffix() const;
	virtual PermissionMask getPermissionMask() const;
	virtual time_t getCreationDate() const;
	virtual BOOL isItemRenameable() const;
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL removeItem();
	virtual BOOL isItemCopyable() const;
	virtual BOOL copyToClipboard() const;
	virtual BOOL cutToClipboard() const;
	virtual BOOL hasChildren() const		{ return FALSE; }
	virtual BOOL isUpToDate() const			{ return TRUE; }

	// override for LLInvFVBridge
	virtual void clearDisplayName()			{ mDisplayName.clear(); }

	LLViewerInventoryItem* getItem() const;

protected:
	virtual BOOL isItemPermissive() const;
	static void buildDisplayName(LLInventoryItem* item, std::string& name);
	mutable std::string mDisplayName;
};

class LLFolderBridge : public LLInvFVBridge
{
	friend class LLInvFVBridge;

public:
	BOOL dragItemIntoFolder(LLInventoryItem* inv_item,
							BOOL drop);
	BOOL dragCategoryIntoFolder(LLInventoryCategory* inv_category,
								BOOL drop);
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void openItem();
	virtual BOOL isItemRenameable() const;
	virtual void selectItem();
	virtual void restoreItem();

	virtual LLFolderType::EType getPreferredType() const;
	virtual LLUIImagePtr getIcon() const;
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL removeItem();
	virtual BOOL isClipboardPasteable() const;
	virtual BOOL isClipboardPasteableAsLink() const;
	virtual BOOL cutToClipboard() const;
	virtual void pasteFromClipboard();
	virtual void pasteLinkFromClipboard();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual BOOL hasChildren() const;
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);

	virtual BOOL isItemRemovable();
	virtual BOOL isItemMovable();
	virtual BOOL isUpToDate() const;

	static void createWearable(LLFolderBridge* bridge,
							   LLWearableType::EType type);

	LLViewerInventoryCategory* getCategory() const;

protected:
	LLFolderBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLInvFVBridge(inventory, uuid),
		mCallingCards(FALSE),
		mWearables(FALSE)
	{}

	// menu callbacks
	static void pasteClipboard(void* user_data);
	static void createNewCategory(void* user_data);

	static void createNewShirt(void* user_data);
	static void createNewPants(void* user_data);
	static void createNewShoes(void* user_data);
	static void createNewSocks(void* user_data);
	static void createNewJacket(void* user_data);
	static void createNewSkirt(void* user_data);
	static void createNewGloves(void* user_data);
	static void createNewUndershirt(void* user_data);
	static void createNewUnderpants(void* user_data);
	static void createNewAlpha(void* user_data);
	static void createNewTattoo(void* user_data);
	static void createNewPhysics(void* user_data);
	static void createNewShape(void* user_data);
	static void createNewSkin(void* user_data);
	static void createNewHair(void* user_data);
	static void createNewEyes(void* user_data);

	BOOL checkFolderForContentsOfType(LLInventoryModel* model,
									  LLInventoryCollectFunctor& typeToCheck);

	void modifyOutfit(bool append, bool replace);

public:
	static LLFolderBridge* sSelf;
	static void staticFolderOptionsMenu();
	void folderOptionsMenu();

private:
	BOOL			mCallingCards;
	BOOL			mWearables;
	LLMenuGL*		mMenu;
	std::vector<std::string> mItems;
	std::vector<std::string> mDisabledItems;
};

// DEPRECATED
class LLScriptBridge : public LLItemBridge
{
	friend class LLInvFVBridge;

public:
	LLUIImagePtr getIcon() const;

protected:
	LLScriptBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}
};

class LLTextureBridge : public LLItemBridge
{
	friend class LLInvFVBridge;

public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();

protected:
	LLTextureBridge(LLInventoryPanel* inventory, const LLUUID& uuid,
					LLInventoryType::EType type)
	:	LLItemBridge(inventory, uuid), mInvType(type) {}
	static std::string sPrefix;
	LLInventoryType::EType mInvType;
};

class LLSoundBridge : public LLItemBridge
{
	friend class LLInvFVBridge;

public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual void previewItem();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	static void openSoundPreview(void*);

protected:
	LLSoundBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}
	static std::string sPrefix;
};

class LLLandmarkBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	static const std::string& prefix()			{ return sPrefix; }
	virtual const std::string& getPrefix()		{ return sPrefix; }
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();

protected:
	LLLandmarkBridge(LLInventoryPanel* inventory,
					 const LLUUID& uuid, U32 flags = 0x00)
	:	LLItemBridge(inventory, uuid)
	{
		mVisited = FALSE;
		if (flags & LLInventoryItem::II_FLAGS_LANDMARK_VISITED)
		{
			mVisited = TRUE;
		}
	}

protected:
	static std::string sPrefix;
	BOOL mVisited;
};

class LLCallingCardBridge;

class LLCallingCardObserver : public LLFriendObserver
{
public:
	LLCallingCardObserver(LLCallingCardBridge* bridge)
	:	mBridgep(bridge)
	{
	}

	virtual ~LLCallingCardObserver()		{ mBridgep = NULL; }
	virtual void changed(U32 mask);

protected:
	LLCallingCardBridge* mBridgep;
};

class LLCallingCardBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix()	{ return sPrefix; }

	virtual std::string getLabelSuffix() const;
	//virtual const std::string& getDisplayName() const;
	virtual LLUIImagePtr getIcon() const;
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void openItem();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	//virtual void renameItem(const std::string& new_name);
	//virtual BOOL removeItem();
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);
	void refreshFolderViewItem();

protected:
	LLCallingCardBridge(LLInventoryPanel* inventory, const LLUUID& uuid);
	~LLCallingCardBridge();

protected:
	static std::string sPrefix;
	LLCallingCardObserver* mObserver;
};

class LLNotecardBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();

protected:
	LLNotecardBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}

protected:
	static std::string sPrefix;
};

class LLGestureBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;

	// Only suffix for gesture items, not task items, because only
	// gestures in your inventory can be active.
	virtual LLFontGL::StyleFlags getLabelStyle() const;
	virtual std::string getLabelSuffix() const;

	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void openItem();
	virtual BOOL removeItem();

	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);

protected:
	LLGestureBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}

protected:
	static std::string sPrefix;
};

class LLAnimationBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();

protected:
	LLAnimationBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}

protected:
	static std::string sPrefix;
};

class LLObjectBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void openItem();
	virtual LLFontGL::StyleFlags getLabelStyle() const;
	virtual std::string getLabelSuffix() const;
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual BOOL isItemRemovable();
	virtual BOOL renameItem(const std::string& new_name);

protected:
	LLObjectBridge(LLInventoryPanel* inventory, const LLUUID& uuid,
				   LLInventoryType::EType type, U32 flags)
	:	LLItemBridge(inventory, uuid), mInvType(type)
	{
		mAttachPt = (flags & 0xff); // low bye of inventory flags

		mIsMultiObject = (flags & LLInventoryItem::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS) ? TRUE
																					   : FALSE;
	}

protected:
	static std::string sPrefix;
	LLInventoryType::EType mInvType;
	U32 mAttachPt;
	BOOL mIsMultiObject;
};

class LLLSLTextBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();

protected:
	LLLSLTextBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}

protected:
	static std::string sPrefix;
};

class LLWearableBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual LLUIImagePtr getIcon() const;
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void openItem();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual LLFontGL::StyleFlags getLabelStyle() const;
	virtual std::string getLabelSuffix() const;
	virtual BOOL isItemRemovable();
	virtual BOOL renameItem(const std::string& new_name);

	// Access to wearOnAvatar() from menu:
	static void onWearOnAvatar(void* userdata);
	static BOOL canWearOnAvatar(void* userdata);
	static void onWearOnAvatarArrived(LLWearable* wearable, void* userdata);
	void wearOnAvatar(bool replace = true);

	// Access to editOnAvatar() from menu:
	static BOOL canEditOnAvatar(void* userdata);
	static void onEditOnAvatar(void* userdata);
	void editOnAvatar();

	static BOOL canRemoveFromAvatar(void* userdata);
	static void onRemoveFromAvatar(void* userdata);
	static void onRemoveFromAvatarArrived(LLWearable* wearable,
										  void* userdata);

protected:
	LLWearableBridge(LLInventoryPanel* inventory,
					 const LLUUID& uuid,
					 LLAssetType::EType asset_type,
					 LLInventoryType::EType inv_type,
					 LLWearableType::EType  wearable_type)
	:	LLItemBridge(inventory, uuid),
		mAssetType(asset_type),
		mInvType(inv_type),
		mWearableType(wearable_type)
		{}

protected:
	LLAssetType::EType		mAssetType;
	LLInventoryType::EType	mInvType;
	LLWearableType::EType	mWearableType;
};

class LLLinkItemBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);

protected:
	LLLinkItemBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}

protected:
	static std::string sPrefix;
};

class LLLinkFolderBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);
	virtual void gotoItem(LLFolderView *folder);

protected:
	LLLinkFolderBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}
	const LLUUID &getFolderID() const;

protected:
	static std::string sPrefix;
};

class LLMeshBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	virtual const std::string& getPrefix() { return sPrefix; }

	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual void previewItem();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual void performAction(LLFolderView* folder, LLInventoryModel* model,
							   std::string action);

protected:
	LLMeshBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid) {}

protected:
	static std::string sPrefix;
};

class LLFindWearables : public LLInventoryCollectFunctor
{
public:
	LLFindWearables() {}
	virtual ~LLFindWearables() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item);
};

#endif // LL_LLINVENTORYBRIDGE_H
