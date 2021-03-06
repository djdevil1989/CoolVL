/**
 * @file llvoavatarself.h
 * @brief Declaration of LLVOAvatarSelf class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLVOAVATARSELF_H
#define LL_LLVOAVATARSELF_H

#include "llviewertexture.h"
#include "llvoavatar.h"

struct LocalTextureData;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLVOAvatarSelf
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLVOAvatarSelf : public LLVOAvatar
{

/********************************************************************************
 **                                                                            **
 **                    INITIALIZATION
 **/

public:
	void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	LLVOAvatarSelf(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp);
	virtual 			~LLVOAvatarSelf();
	virtual void		markDead();
	virtual void 		initInstance(); // Called after construction to initialize the class.

protected:
	/*virtual*/ BOOL	loadAvatar();
	BOOL				loadAvatarSelf();
	BOOL				buildSkeletonSelf(const LLVOAvatarSkeletonInfo *info);
	BOOL				buildMenus();
	/*virtual*/ BOOL	loadLayersets();

/**                    Initialization
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    INHERITED
 **/

	//--------------------------------------------------------------------
	// LLViewerObject interface and related
	//--------------------------------------------------------------------
public:
	/*virtual*/ void 		updateRegion(LLViewerRegion *regionp);
	/*virtual*/ BOOL   	 	idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time);

	//--------------------------------------------------------------------
	// LLCharacter interface and related
	//--------------------------------------------------------------------
public:
	/*virtual*/ void 		stopMotionFromSource(const LLUUID& source_id);
	/*virtual*/ void 		requestStopMotion(LLMotion* motion);
	/*virtual*/ LLJoint*	getJoint(const std::string &name);

	/*virtual*/ BOOL setVisualParamWeight(LLVisualParam *which_param, F32 weight, BOOL upload_bake = FALSE);
	/*virtual*/ BOOL setVisualParamWeight(const char* param_name, F32 weight, BOOL upload_bake = FALSE);
	/*virtual*/ BOOL setVisualParamWeight(S32 index, F32 weight, BOOL upload_bake = FALSE);
	/*virtual*/ void idleUpdateAppearanceAnimation();

	/*virtual*/ U32  processUpdateMessage(LLMessageSystem* mesgsys,
										  void** user_data,
										  U32 block_num,
										  const EObjectUpdateType update_type,
										  LLDataPacker* dp);

private:
	// helper function. Passed in param is assumed to be in avatar's parameter list.
	BOOL setParamWeight(LLViewerVisualParam *param, F32 weight, BOOL upload_bake = FALSE);

/**                    Initialization
 **                                                                            **
 *******************************************************************************/

private:
	LLUUID mInitialBakeIDs[6];
	bool mInitialBakesLoaded;

/********************************************************************************
 **                                                                            **
 **                    STATE
 **/

public:
	/*virtual*/ bool 	isSelf() const { return true; }

	//--------------------------------------------------------------------
	// Updates
	//--------------------------------------------------------------------
public:
	/*virtual*/ BOOL 	updateCharacter(LLAgent &agent);
	/*virtual*/ void 	idleUpdateTractorBeam();

	//--------------------------------------------------------------------
	// Loading state
	//--------------------------------------------------------------------
public:
	/*virtual*/ BOOL	getIsCloud();

	//--------------------------------------------------------------------
	// Region state
	//--------------------------------------------------------------------
	void			resetRegionCrossingTimer()	{ mRegionCrossingTimer.reset();	}

private:
	U64				mLastRegionHandle;
	LLFrameTimer	mRegionCrossingTimer;
	S32				mRegionCrossingCount;

/**                    State
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    RENDERING
 **/

	//--------------------------------------------------------------------
	// Render beam
	//--------------------------------------------------------------------
protected:
	BOOL 			needsRenderBeam();

private:
	LLPointer<LLHUDEffectSpiral> mBeam;
	LLFrameTimer mBeamTimer;

	//--------------------------------------------------------------------
	// LLVOAvatar Constants
	//--------------------------------------------------------------------
public:
	/*virtual*/ LLViewerTexture::EBoostLevel 	getAvatarBoostLevel() const { return LLViewerTexture::BOOST_AVATAR_SELF; }
	/*virtual*/ LLViewerTexture::EBoostLevel 	getAvatarBakedBoostLevel() const { return LLViewerTexture::BOOST_AVATAR_BAKED_SELF; }
	/*virtual*/ S32 							getTexImageSize() const { return LLVOAvatar::getTexImageSize() * 4; }

/**                    Rendering
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    TEXTURES
 **/

	//--------------------------------------------------------------------
	// Loading status
	//--------------------------------------------------------------------
public:
	/*virtual*/ bool	hasPendingBakedUploads() const;
	S32					getLocalDiscardLevel(LLVOAvatarDefines::ETextureIndex type, U32 index) const;
	bool				areTexturesCurrent() const;
	BOOL				isLocalTextureDataAvailable(const LLTexLayerSet* layerset) const;
	BOOL				isLocalTextureDataFinal(const LLTexLayerSet* layerset) const;
	BOOL				isBakedTextureFinal(const LLVOAvatarDefines::EBakedTextureIndex index) const;
	// If you want to check all textures of a given type, pass gAgentWearables.getWearableCount() for index
	/*virtual*/ BOOL    isTextureDefined(LLVOAvatarDefines::ETextureIndex type, U32 index) const;
	/*virtual*/ BOOL	isTextureVisible(LLVOAvatarDefines::ETextureIndex type, U32 index = 0) const;
	/*virtual*/ BOOL	isTextureVisible(LLVOAvatarDefines::ETextureIndex type, LLWearable *wearable) const;

	//--------------------------------------------------------------------
	// Local Textures
	//--------------------------------------------------------------------
public:
	BOOL				getLocalTextureGL(LLVOAvatarDefines::ETextureIndex type, LLViewerTexture** image_gl_pp, U32 index) const;
	LLViewerFetchedTexture*	getLocalTextureGL(LLVOAvatarDefines::ETextureIndex type, U32 index) const;
	const LLUUID&		getLocalTextureID(LLVOAvatarDefines::ETextureIndex type, U32 index) const;
	void				setLocalTextureTE(U8 te, LLViewerTexture* image, U32 index);
	/*virtual*/ void	setLocalTexture(LLVOAvatarDefines::ETextureIndex type, LLViewerTexture* tex, BOOL baked_version_exits, U32 index);

protected:
	/*virtual*/ void	setBakedReady(LLVOAvatarDefines::ETextureIndex type, BOOL baked_version_exists, U32 index);
	void				localTextureLoaded(BOOL succcess, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
	void				getLocalTextureByteCount(S32* gl_byte_count) const;
	/*virtual*/ void	addLocalTextureStats(LLVOAvatarDefines::ETextureIndex i, LLViewerFetchedTexture* imagep, F32 texel_area_ratio, BOOL rendered, BOOL covered_by_baked, U32 index);
	LLLocalTextureObject* getLocalTextureObject(LLVOAvatarDefines::ETextureIndex i, U32 index) const;

private:
	static void			onLocalTextureLoaded(BOOL succcess, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);

	/*virtual*/	void	setImage(const U8 te, LLViewerTexture *imagep, const U32 index); 
	/*virtual*/ LLViewerTexture* getImage(const U8 te, const U32 index) const;

	//--------------------------------------------------------------------
	// Baked textures
	//--------------------------------------------------------------------
public:
	LLVOAvatarDefines::ETextureIndex getBakedTE(const LLTexLayerSet* layerset) const;
	void				setNewBakedTexture(LLVOAvatarDefines::EBakedTextureIndex i, const LLUUID &uuid);
	void				setNewBakedTexture(LLVOAvatarDefines::ETextureIndex i, const LLUUID& uuid);
	void				setCachedBakedTexture(LLVOAvatarDefines::ETextureIndex i, const LLUUID& uuid);
	void				forceBakeAllTextures(bool slam_for_debug = false);
	static void			processRebakeAvatarTextures(LLMessageSystem* msg, void**);
	BOOL				isUsingBakedTextures() const; // e.g. false if in appearance edit mode
protected:
	/*virtual*/ void	removeMissingBakedTextures();

	//--------------------------------------------------------------------
	// Layers
	//--------------------------------------------------------------------
public:
	void 				requestLayerSetUploads();
	void				requestLayerSetUpload(LLVOAvatarDefines::EBakedTextureIndex i);
	void				requestLayerSetUpdate(LLVOAvatarDefines::ETextureIndex i);
	LLTexLayerSet*		getLayerSet(LLVOAvatarDefines::ETextureIndex index) const;
	LLTexLayerSet* 		getLayerSet(LLVOAvatarDefines::EBakedTextureIndex baked_index) const;

	//--------------------------------------------------------------------
	// Composites
	//--------------------------------------------------------------------
public:
	/*virtual*/ void	invalidateComposite(LLTexLayerSet* layerset, BOOL upload_result);
	/*virtual*/ void	invalidateAll();
	/*virtual*/ void	setCompositeUpdatesEnabled(bool b); // only works for self
	/*virtual*/ void	setCompositeUpdatesEnabled(U32 index, bool b);
	/*virtual*/ bool	isCompositeUpdateEnabled(U32 index);
	void				setupComposites();
	void				updateComposites();

	const LLUUID&		grabBakedTexture(LLVOAvatarDefines::EBakedTextureIndex baked_index) const;
	BOOL				canGrabBakedTexture(LLVOAvatarDefines::EBakedTextureIndex baked_index) const;

	//--------------------------------------------------------------------
	// Scratch textures (used for compositing)
	//--------------------------------------------------------------------
public:
	static void							deleteScratchTextures();

private:
	static S32 							sScratchTexBytes;
	static LLMap< LLGLenum, LLGLuint*>	sScratchTexNames;
	static LLMap< LLGLenum, F32*>		sScratchTexLastBindTime;

/**                    Textures
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    MESHES
 **/
protected:
	/*virtual*/ void   restoreMeshData();

/**                    Meshes
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    WEARABLES
 **/

public:
	/*virtual*/ BOOL	isWearingWearableType(LLWearableType::EType type) const;
	void				wearableUpdated(LLWearableType::EType type, BOOL upload_result);

protected:
	U32 getNumWearables(LLVOAvatarDefines::ETextureIndex i) const;

	//--------------------------------------------------------------------
	// Attachments
	//--------------------------------------------------------------------
public:
	void 				updateAttachmentVisibility(U32 camera_mode);
	BOOL 				isWearingAttachment(const LLUUID& inv_item_id) const;
#if 0
	// Attachment requests tracking: not implemented in the Cool VL Viewer for now.
	BOOL				attachmentWasRequested(const LLUUID& inv_item_id) const;
	void				addAttachmentRequest(const LLUUID& inv_item_id);
	void				removeAttachmentRequest(const LLUUID& inv_item_id);
#endif
	LLViewerObject* 	getWornAttachment(const LLUUID& inv_item_id);
	const std::string   getAttachedPointName(const LLUUID& inv_item_id) const;
	/*virtual*/ const LLViewerJointAttachment* attachObject(LLViewerObject *viewer_object);
	/*virtual*/ BOOL 	detachObject(LLViewerObject *viewer_object);
	static BOOL			detachAttachmentIntoInventory(const LLUUID& item_id);

private:
	// Track attachments that have been requested but have not arrived yet.
	mutable std::map<LLUUID,LLTimer> mAttachmentRequests;

	//--------------------------------------------------------------------
	// HUDs
	//--------------------------------------------------------------------
private:
	LLViewerJoint* 		mScreenp; // special purpose joint for HUD attachments

/**                    Attachments
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    APPEARANCE
 **/

public:
	static void		onCustomizeStart();
	static void		onCustomizeEnd();

	//--------------------------------------------------------------------
	// Visibility
	//--------------------------------------------------------------------
public:
	bool			sendAppearanceMessage(LLMessageSystem *mesgsys) const;

/**                    Appearance
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    DIAGNOSTICS
 **/

	//--------------------------------------------------------------------
	// General
	//--------------------------------------------------------------------
public:
	static void		dumpTotalLocalTextureByteCount();
	void			dumpLocalTextures() const;
	static void		dumpScratchTextureByteCount();

	//--------------------------------------------------------------------
	// Avatar Rez Metrics
	//--------------------------------------------------------------------
public:
	struct LLAvatarTexData
	{
		LLAvatarTexData(const LLUUID& id, LLVOAvatarDefines::ETextureIndex index)
		: 	mAvatarID(id), 
			mIndex(index) 
		{}
		LLUUID								mAvatarID;
		LLVOAvatarDefines::ETextureIndex	mIndex;
	};

/**                    Diagnostics
 **                                                                            **
 *******************************************************************************/

};

// Globals
extern bool							gAttachmentsListDirty;
extern LLFrameTimer					gAttachmentsTimer;
extern LLPointer<LLVOAvatarSelf>	gAgentAvatarp;

inline BOOL isAgentAvatarValid()
{
	return gAgentAvatarp.notNull() && !gAgentAvatarp->isDead() &&
		   gAgentAvatarp->getRegion();
}

#endif // LL_VO_AVATARSELF_H
