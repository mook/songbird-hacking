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

/* base class */
#include "sbDeviceLibrary.h"

/* mozilla interfaces */
#include <nsIFileURL.h>
#include <nsIPrefService.h>
#include <nsIWritablePropertyBag2.h>

/* nspr headers */
#include <prlog.h>
#include <prprf.h>
#include <prtime.h>

/* mozilla headers */
#include <nsAppDirectoryServiceDefs.h>
#include <nsArrayUtils.h>
#include <nsAutoLock.h>
#include <nsCOMArray.h>
#include <nsComponentManagerUtils.h>
#include <nsDirectoryServiceUtils.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>
#include <nsThreadUtils.h>
#include <nsXPCOM.h>
#include <nsXPCOMCIDInternal.h>

/* songbird interfaces */
#include <sbIDevice.h>
#include <sbIDeviceContent.h>
#include <sbILibraryFactory.h>
#include <sbILibraryManager.h>
#include <sbIPropertyArray.h>

/* songbird headers */
#include <sbLocalDatabaseCID.h>
#include <sbMemoryUtils.h>
#include <sbProxyUtils.h>


NS_IMPL_THREADSAFE_ISUPPORTS7(sbDeviceLibrary,
                              sbIMediaListListener,
                              sbILocalDatabaseMediaListCopyListener,
                              sbIDeviceLibrary,
                              sbILibrary,
                              sbIMediaList,
                              sbIMediaItem,
                              sbILibraryResource)

#define PREF_SYNC_BRANCH    "songbird.device.library."
#define PREF_SYNC_MGMTTYPE  "mgmtType"
#define PREF_SYNC_LISTS     "playlist."

/**
 * To log this module, set the following environment variable:
 *   NSPR_LOG_MODULES=sbDeviceLibrary:5
 */
#ifdef PR_LOGGING
static PRLogModuleInfo* gDeviceLibraryLog = nsnull;
#define TRACE(args) PR_LOG(gDeviceLibraryLog, PR_LOG_DEBUG, args)
#define LOG(args)   PR_LOG(gDeviceLibraryLog, PR_LOG_WARN, args)
#else
#define TRACE(args) /* nothing */
#define LOG(args)   /* nothing */
#endif /* PR_LOGGING */

sbDeviceLibrary::sbDeviceLibrary(sbIDevice* aDevice)
  : mLock(nsnull),
    mDevice(aDevice)
{
#ifdef PR_LOGGING
  if (!gDeviceLibraryLog) {
    gDeviceLibraryLog = PR_NewLogModule("sbDeviceLibrary");
  }
#endif
  TRACE(("DeviceLibrary[0x%.8x] - Constructed", this));
}

sbDeviceLibrary::~sbDeviceLibrary()
{
  Finalize();

  if(mLock) {
    nsAutoLock::DestroyLock(mLock);
  }
  
  TRACE(("DeviceLibrary[0x%.8x] - Destructed", this));
}

NS_IMETHODIMP
sbDeviceLibrary::Initialize(const nsAString& aLibraryId)
{
  NS_ENSURE_FALSE(mLock, NS_ERROR_ALREADY_INITIALIZED);
  mLock = nsAutoLock::NewLock(__FILE__ "sbDeviceLibrary::mLock");
  NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);
  PRBool succeeded = mListeners.Init();
  NS_ENSURE_TRUE(succeeded, NS_ERROR_OUT_OF_MEMORY);
  return CreateDeviceLibrary(aLibraryId, nsnull);
}

NS_IMETHODIMP
sbDeviceLibrary::Finalize()
{
  // Get and clear the device library.
  nsCOMPtr<sbILibrary> deviceLibrary;
  {
    nsAutoLock lock(mLock);
    deviceLibrary = mDeviceLibrary;
    mDeviceLibrary = nsnull;
    // remove the listeners
    mListeners.Clear();
  }

  if (deviceLibrary)
    UnregisterDeviceLibrary(deviceLibrary);
  
  // let go of the owner device
  mDevice = nsnull;

  return NS_OK;
}

nsresult 
sbDeviceLibrary::CreateDeviceLibrary(const nsAString &aDeviceIdentifier,
                                     nsIURI *aDeviceDatabaseURI)
{
  nsresult rv;
  nsCOMPtr<sbILibraryFactory> libraryFactory = 
    do_GetService(SB_LOCALDATABASE_LIBRARYFACTORY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWritablePropertyBag2> libraryProps = 
    do_CreateInstance(NS_HASH_PROPERTY_BAG_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> libraryFile;
  if(aDeviceDatabaseURI) {
    //Use preferred database location.
    nsCOMPtr<nsIFileURL> furl = do_QueryInterface(aDeviceDatabaseURI, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = furl->GetFile(getter_AddRefs(libraryFile));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    //No preferred database location, fetch default location.
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, 
      getter_AddRefs(libraryFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = libraryFile->Append(NS_LITERAL_STRING("db"));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool exists = PR_FALSE;
    rv = libraryFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if(!exists) {
      rv = libraryFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsAutoString filename(aDeviceIdentifier);
    filename.AppendLiteral(".db");

    rv = libraryFile->Append(filename);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = libraryProps->SetPropertyAsInterface(NS_LITERAL_STRING("databaseFile"), 
                                            libraryFile);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  {
    nsCAutoString str;
    if(NS_SUCCEEDED(libraryFile->GetNativePath(str))) {
      LOG(("Attempting to create device library with file path %s", str.get()));
    }
  }
#endif

  rv = libraryFactory->CreateLibrary(libraryProps,
                                     getter_AddRefs(mDeviceLibrary));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIMediaList> list;
  list = do_QueryInterface(mDeviceLibrary, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = list->AddListener(this,
                         PR_FALSE,
                         sbIMediaList::LISTENER_FLAGS_ITEMADDED |
                         sbIMediaList::LISTENER_FLAGS_AFTERITEMREMOVED |
                         sbIMediaList::LISTENER_FLAGS_ITEMUPDATED |
                         sbIMediaList::LISTENER_FLAGS_LISTCLEARED,
                         nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbILocalDatabaseSimpleMediaList> simpleList;
  simpleList = do_QueryInterface(list, &rv);
  
  if(NS_SUCCEEDED(rv)) {
    rv = simpleList->SetCopyListener(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    NS_WARN_IF_FALSE(rv, 
      "Failed to get sbILocalDatabaseSimpleMediaList interface. Copy Listener disabled.");
  }

  rv = RegisterDeviceLibrary(mDeviceLibrary);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult 
sbDeviceLibrary::RegisterDeviceLibrary(sbILibrary* aDeviceLibrary)
{
  NS_ENSURE_ARG_POINTER(aDeviceLibrary);
  
  nsresult rv;
  nsCOMPtr<sbILibraryManager> libraryManager;
  libraryManager = 
    do_GetService("@songbirdnest.com/Songbird/library/Manager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return libraryManager->RegisterLibrary(aDeviceLibrary, PR_FALSE);
}

nsresult 
sbDeviceLibrary::UnregisterDeviceLibrary(sbILibrary* aDeviceLibrary)
{
  NS_ENSURE_ARG_POINTER(aDeviceLibrary);

  nsresult rv;
  nsCOMPtr<sbILibraryManager> libraryManager;
  libraryManager = 
    do_GetService("@songbirdnest.com/Songbird/library/Manager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return libraryManager->UnregisterLibrary(aDeviceLibrary);
}

nsresult
sbDeviceLibrary::GetSyncPrefBranch(nsIPrefBranch** _retval)
{
  nsresult rv;
  
  // Get the branch for this library
  nsCOMPtr<nsIPrefService> prefService =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString prefKey(PREF_SYNC_BRANCH);
  nsString guid;
  rv = mDeviceLibrary->GetGuid(guid);
  NS_ENSURE_SUCCESS(rv, rv);
  prefKey.Append(NS_ConvertUTF16toUTF8(guid).get());
  prefKey.Append(".sync.");
  
  nsCOMPtr<nsIPrefBranch> syncListsBranch;
  return prefService->GetBranch(prefKey.get(), _retval);
}

/**
 * sbIDeviceLibrary
 */

NS_IMETHODIMP
sbDeviceLibrary::GetMgmtType(PRUint32 *aMgmtType)
{
  NS_ENSURE_ARG_POINTER(aMgmtType);

  nsresult rv;
  nsCString mgmtTypeKey(PREF_SYNC_MGMTTYPE);

  nsCOMPtr<nsIPrefBranch> syncListsBranch;
  rv = GetSyncPrefBranch(getter_AddRefs(syncListsBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 prefType;
  rv = syncListsBranch->GetPrefType(mgmtTypeKey.get(), &prefType);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 signedMgmtType = sbIDeviceLibrary::MGMT_TYPE_MANUAL;
  if (prefType == nsIPrefBranch::PREF_INT) {
    rv = syncListsBranch->GetIntPref( mgmtTypeKey.get(), &signedMgmtType );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Double check that it is a valid number
  if ( (signedMgmtType < sbIDeviceLibrary::MGMT_TYPE_MANUAL) ||
       (signedMgmtType > sbIDeviceLibrary::MGMT_TYPE_SYNC_PLAYLISTS) ) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *aMgmtType = signedMgmtType;
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::SetMgmtType(PRUint32 aMgmtType)
{
  nsresult rv;
  
  // Check we are setting to a valid number
  if ( (aMgmtType < sbIDeviceLibrary::MGMT_TYPE_MANUAL) ||
       (aMgmtType > sbIDeviceLibrary::MGMT_TYPE_SYNC_PLAYLISTS) ) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsCString mgmtTypeKey(PREF_SYNC_MGMTTYPE);

  nsCOMPtr<nsIPrefBranch> syncListsBranch;
  rv = GetSyncPrefBranch(getter_AddRefs(syncListsBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 oldPrefType;
  rv = syncListsBranch->GetPrefType(mgmtTypeKey.get(), &oldPrefType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (oldPrefType != nsIPrefBranch::PREF_INVALID &&
      oldPrefType != nsIPrefBranch::PREF_INT) {
    rv = syncListsBranch->ClearUserPref(mgmtTypeKey.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return syncListsBranch->SetIntPref( mgmtTypeKey.get(), aMgmtType );
}

NS_IMETHODIMP
sbDeviceLibrary::GetSyncPlaylistList(nsIArray **_retval)
{
  NS_ENSURE_ARG_POINTER( _retval );
  nsresult rv;

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<sbILibraryManager> libManager =
      do_GetService("@songbirdnest.com/Songbird/library/Manager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbILibrary> mainLibrary;
  rv = libManager->GetMainLibrary(getter_AddRefs(mainLibrary));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 listLength;
  char** listGuids;

  nsCOMPtr<nsIPrefBranch> syncListsBranch;
  rv = GetSyncPrefBranch(getter_AddRefs(syncListsBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = syncListsBranch->GetChildList(PREF_SYNC_LISTS, &listLength, &listGuids);
  NS_ENSURE_SUCCESS(rv, rv);
  
  sbAutoFreeXPCOMArray<char**> autoFree(listLength, listGuids);
  
  for ( PRUint32 index = 0; index < listLength; index++ ) {
    nsCAutoString guidPref(listGuids[index]);
    NS_ASSERTION(StringBeginsWith(guidPref, NS_LITERAL_CSTRING(PREF_SYNC_LISTS)),
                 "Bad guid string!");

    // We want to extract the guid from the pref name, if it starts with the
    // PREF_SYNC_LISTS text
    PRUint32 branchLength = NS_LITERAL_CSTRING(PREF_SYNC_LISTS).Length();
    PRInt32 firstDotIndex = guidPref.FindChar('.', branchLength);
    NS_ASSERTION(firstDotIndex != -1, "Bad guid string!");
    PRUint32 guidLength = firstDotIndex - branchLength;
    NS_ASSERTION(guidLength > 0, "Bad guid string!");
    nsCAutoString guidString(Substring(guidPref, branchLength, guidLength));
  
    nsCOMPtr<sbIMediaItem> syncPlaylistItem;
    rv = mainLibrary->GetItemByGuid(NS_ConvertUTF8toUTF16(guidString),
                                    getter_AddRefs(syncPlaylistItem));
    if ( rv == NS_ERROR_NOT_AVAILABLE ) {
        // Remove it from the preferences
        rv = syncListsBranch->ClearUserPref( guidPref.get() );
        NS_ENSURE_SUCCESS(rv, rv);
    } else if( NS_SUCCEEDED(rv) ) {
        nsCOMPtr<sbIMediaList> syncPlaylist;
        rv = syncPlaylistItem->QueryInterface(NS_GET_IID( sbIMediaList),
                                              getter_AddRefs(syncPlaylist));
         NS_ENSURE_SUCCESS(rv, rv);
         
        rv = array->AppendElement(syncPlaylist, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  NS_ADDREF(*_retval = array);
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::SetSyncPlaylistList(nsIArray *aPlaylistList)
{
  NS_ENSURE_ARG_POINTER( aPlaylistList );
  nsresult rv;
  
  nsCOMPtr<nsIPrefBranch> syncListsBranch;
  rv = GetSyncPrefBranch(getter_AddRefs(syncListsBranch));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = syncListsBranch->DeleteBranch(PREF_SYNC_LISTS);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length;
  rv = aPlaylistList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<sbIMediaList> mediaList = do_QueryElementAt(aPlaylistList, i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = AddToSyncPlaylistList(mediaList);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::AddToSyncPlaylistList(sbIMediaList *aPlaylist)
{
  NS_ENSURE_ARG_POINTER(aPlaylist);
  nsresult rv;
  
  // Get the guid of the list
  nsString guid;
  rv = aPlaylist->GetGuid(guid);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> syncListsBranch;
  rv = GetSyncPrefBranch(getter_AddRefs(syncListsBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString guidListKey(PREF_SYNC_LISTS);
  guidListKey.Append(NS_ConvertUTF16toUTF8(guid).get());
  rv = syncListsBranch->SetBoolPref( guidListKey.get(), PR_TRUE );
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::Sync()
{
  nsresult rv;
  
  nsCOMPtr<sbIDevice> device;
  rv = GetDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(device,
               "sbDeviceLibrary::GetDevice returned success with no device");
  
  nsCOMPtr<sbILibraryManager> libraryManager =
    do_GetService("@songbirdnest.com/Songbird/library/Manager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<sbILibrary> mainLib;
  rv = libraryManager->GetMainLibrary(getter_AddRefs(mainLib));
  NS_ENSURE_SUCCESS(rv, rv);
  
  #if DEBUG
    // sanity check that we're using the right device
    { /* scope */
      nsCOMPtr<sbIDeviceContent> content;
      rv = device->GetContent(getter_AddRefs(content));
      NS_ENSURE_SUCCESS(rv, rv);
      
      nsCOMPtr<nsIArray> libraries;
      rv = content->GetLibraries(getter_AddRefs(libraries));
      NS_ENSURE_SUCCESS(rv, rv);
      
      PRUint32 libraryCount;
      rv = libraries->GetLength(&libraryCount);
      NS_ENSURE_SUCCESS(rv, rv);
      
      PRBool found = PR_FALSE;
      for (PRUint32 index = 0; index < libraryCount; ++index) {
        nsCOMPtr<sbIDeviceLibrary> library =
          do_QueryElementAt(libraries, index, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        
        if (SameCOMIdentity(NS_ISUPPORTS_CAST(sbILibrary*, this), library)) {
          found = PR_TRUE;
          break;
        }
      }
      NS_ASSERTION(found,
                   "sbDeviceLibrary has device that doesn't hold this library");
    }
  #endif /* DEBUG */
  
  nsCOMPtr<nsIWritablePropertyBag2> requestParams =
    do_CreateInstance(NS_HASH_PROPERTY_BAG_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = requestParams->SetPropertyAsInterface(NS_LITERAL_STRING("item"),
                                             mainLib);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = requestParams->SetPropertyAsInterface(NS_LITERAL_STRING("list"),
                                             NS_ISUPPORTS_CAST(sbIMediaList*, this));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = device->SubmitRequest(sbIDevice::REQUEST_SYNC, requestParams);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::AddDeviceLibraryListener(sbIDeviceLibraryListener* aListener)
{
  TRACE(("sbDeviceLibrary[0x%x] - AddListener", this));
  NS_ENSURE_ARG_POINTER(aListener);

  {
    nsAutoLock lock(mLock);

    if (mListeners.Get(aListener, nsnull)) {
      NS_WARNING("Trying to add a listener twice!");
      return NS_OK;
    }
  }

  // Make a proxy for the listener that will always send callbacks to the
  // current thread.
  nsCOMPtr<sbIDeviceLibraryListener> proxy;
  nsresult rv = SB_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                                     NS_GET_IID(sbIDeviceLibraryListener),
                                     aListener,
                                     NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                                     getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoLock lock(mLock);

  // Add the proxy to the hash table, using the listener as the key.
  PRBool success = mListeners.Put(aListener, proxy);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::RemoveDeviceLibraryListener(sbIDeviceLibraryListener* aListener)
{
  TRACE(("sbLibraryManager[0x%x] - RemoveListener", this));
  NS_ENSURE_ARG_POINTER(aListener);

  nsAutoLock lock(mLock);

  #ifdef DEBUG
    if (!mListeners.Get(aListener, nsnull)) {
      NS_WARNING("Trying to remove a listener that was never added!");
    }
  #endif
  mListeners.Remove(aListener);

  return NS_OK;
}

/**
 * \brief This callback adds the enumerated startup libraries to an nsCOMArray.
 *
 * \param aKey      - An nsAString representing the GUID of the library.
 * \param aEntry    - An sbIDeviceLibraryListener entry.
 * \param aUserData - An nsCOMArray to hold the enumerated pointers.
 *
 * \return PL_DHASH_NEXT on success
 * \return PL_DHASH_STOP on failure
 */
/* static */ PLDHashOperator PR_CALLBACK
sbDeviceLibrary::AddListenersToCOMArrayCallback(nsISupportsHashKey::KeyType aKey,
                                                sbIDeviceLibraryListener* aEntry,
                                                void* aUserData)
{
  NS_ASSERTION(aKey, "Null key in hashtable!");
  NS_ASSERTION(aEntry, "Null entry in hashtable!");

  nsCOMArray<sbIDeviceLibraryListener>* array =
    static_cast<nsCOMArray<sbIDeviceLibraryListener>*>(aUserData);

  PRBool success = array->AppendObject(aEntry);
  NS_ENSURE_TRUE(success, PL_DHASH_STOP);

  return PL_DHASH_NEXT;
}

#define SB_NOTIFY_LISTENERS(call)                                             \
  nsCOMArray<sbIDeviceLibraryListener> listeners;                             \
  {                                                                           \
    nsAutoLock lock(mLock);                                                   \
    mListeners.EnumerateRead(AddListenersToCOMArrayCallback, &listeners);     \
  }                                                                           \
                                                                              \
  PRInt32 count = listeners.Count();                                          \
  for (PRInt32 index = 0; index < count; index++) {                           \
    nsCOMPtr<sbIDeviceLibraryListener> listener = listeners.ObjectAt(index);  \
    NS_ASSERTION(listener, "Null listener!");                                 \
    listener->call;                                                           \
  }                                                                           \

#define SB_NOTIFY_LISTENERS_ASK_PERMISSION(call)                              \
  PRBool mShouldProcceed = PR_TRUE;                                           \
  PRBool mPerformAction = PR_TRUE;                                            \
                                                                              \
  nsCOMArray<sbIDeviceLibraryListener> listeners;                             \
  {                                                                           \
    nsAutoLock lock(mLock);                                                   \
    mListeners.EnumerateRead(AddListenersToCOMArrayCallback, &listeners);     \
  }                                                                           \
                                                                              \
  PRInt32 count = listeners.Count();                                          \
  for (PRInt32 index = 0; index < count; index++) {                           \
    nsCOMPtr<sbIDeviceLibraryListener> listener = listeners.ObjectAt(index);  \
    NS_ASSERTION(listener, "Null listener!");                                 \
    listener->call/*(param, ..., &mShouldProcceed)*/;                         \
    if (!mShouldProcceed) {                                                   \
      /* Abort as soon as one returns false */                                \
      mPerformAction = PR_FALSE;                                              \
      break;                                                                  \
    }                                                                         \
  }                                                                           \


NS_IMETHODIMP
sbDeviceLibrary::OnBatchBegin(sbIMediaList* aMediaList)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnBatchBegin", this));
  
  SB_NOTIFY_LISTENERS(OnBatchBegin(aMediaList));
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnBatchEnd(sbIMediaList* aMediaList)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnBatchEnd", this));
  
  SB_NOTIFY_LISTENERS(OnBatchEnd(aMediaList));
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnItemAdded(sbIMediaList* aMediaList,
                             sbIMediaItem* aMediaItem,
                             PRUint32 aIndex,
                             PRBool* aNoMoreForBatch)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnItemAdded", this));
  
  SB_NOTIFY_LISTENERS(OnItemAdded(aMediaList,
                                  aMediaItem,
                                  aIndex,
                                  aNoMoreForBatch));

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnBeforeItemRemoved(sbIMediaList* aMediaList,
                                     sbIMediaItem* aMediaItem,
                                     PRUint32 aIndex,
                                     PRBool* aNoMoreForBatch)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnBeforeItemRemoved", this));

  SB_NOTIFY_LISTENERS(OnBeforeItemRemoved(aMediaList,
                                          aMediaItem,
                                          aIndex,
                                          aNoMoreForBatch));
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnAfterItemRemoved(sbIMediaList* aMediaList,
                                    sbIMediaItem* aMediaItem,
                                    PRUint32 aIndex,
                                    PRBool* aNoMoreForBatch)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnAfterItemRemoved", this));

  SB_NOTIFY_LISTENERS(OnAfterItemRemoved(aMediaList,
                                         aMediaItem,
                                         aIndex,
                                         aNoMoreForBatch));
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnItemUpdated(sbIMediaList *aMediaList,
                               sbIMediaItem *aMediaItem,
                               sbIPropertyArray* aProperties,
                               PRBool* aNoMoreForBatch)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnItemUpdated", this));

  SB_NOTIFY_LISTENERS(OnItemUpdated(aMediaList,
                                    aMediaItem,
                                    aProperties,
                                    aNoMoreForBatch));
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnItemMoved(sbIMediaList *aMediaList,
                             PRUint32 aFromIndex,
                             PRUint32 aToIndex,
                             PRBool* aNoMoreForBatch)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnItemMoved", this));

  SB_NOTIFY_LISTENERS(OnItemMoved(aMediaList,
                                  aFromIndex,
                                  aToIndex,
                                  aNoMoreForBatch));
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnListCleared(sbIMediaList *aMediaList,
                               PRBool* aNoMoreForBatch)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnListCleared", this));

  SB_NOTIFY_LISTENERS(OnListCleared(aMediaList,
                                    aNoMoreForBatch));
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceLibrary::OnItemCopied(sbIMediaItem *aSourceItem, 
                              sbIMediaItem *aDestItem)
{
  TRACE(("sbDeviceLibrary[0x%x] - OnItemCopied", this));

  SB_NOTIFY_LISTENERS(OnItemCopied(aSourceItem, aDestItem));

  return NS_OK;
}

/*
 * See sbILibrary
 */
NS_IMETHODIMP 
sbDeviceLibrary::CreateMediaItem(nsIURI *aContentUri,
                                 sbIPropertyArray *aProperties,
                                 PRBool aAllowDuplicates,
                                 sbIMediaItem **_retval)
{
  NS_ASSERTION(mDeviceLibrary, "mDeviceLibrary is null, call init first.");
  SB_NOTIFY_LISTENERS_ASK_PERMISSION(OnBeforeCreateMediaItem(aContentUri,
                                                             aProperties,
                                                             aAllowDuplicates,
                                                             &mShouldProcceed));
  if (mPerformAction) {
    nsresult rv;
    nsCOMPtr<sbILibrary> lib;
    rv = SB_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                              NS_GET_IID(sbILibrary),
                              mDeviceLibrary,
                              NS_PROXY_SYNC,
                              getter_AddRefs(lib));
    NS_ENSURE_SUCCESS(rv, rv);
    
    return lib->CreateMediaItem(aContentUri,
                                aProperties,
                                aAllowDuplicates,
                                _retval);
  } else {
    return NS_OK;
  }
}

/*
 * See sbILibrary
 */
NS_IMETHODIMP 
sbDeviceLibrary::CreateMediaList(const nsAString & aType,
                                 sbIPropertyArray *aProperties,
                                 sbIMediaList **_retval)
{
  NS_ASSERTION(mDeviceLibrary, "mDeviceLibrary is null, call init first.");
  SB_NOTIFY_LISTENERS_ASK_PERMISSION(OnBeforeCreateMediaList(aType, 
                                                             aProperties,
                                                             &mShouldProcceed));
  if (mPerformAction) {
    return mDeviceLibrary->CreateMediaList(aType, aProperties, _retval);
  } else {
    return NS_OK;
  }
}

/*
 * See sbILibrary
 */
NS_IMETHODIMP
sbDeviceLibrary::GetDevice(sbIDevice **aDevice)
{
  NS_ENSURE_ARG_POINTER(aDevice);
  NS_IF_ADDREF(*aDevice = mDevice);
  return *aDevice ? NS_OK : NS_ERROR_UNEXPECTED;
}

/*
 * See sbIMediaList
 */
NS_IMETHODIMP 
sbDeviceLibrary::Add(sbIMediaItem *aMediaItem)
{
  NS_ASSERTION(mDeviceLibrary, "mDeviceLibrary is null, call init first.");
  SB_NOTIFY_LISTENERS_ASK_PERMISSION(OnBeforeAdd(aMediaItem, &mShouldProcceed));
  if (mPerformAction) {
    return mDeviceLibrary->Add(aMediaItem);
  } else {
    return NS_OK;
  }
}

/*
 * See sbIMediaList
 */
NS_IMETHODIMP 
sbDeviceLibrary::AddAll(sbIMediaList *aMediaList)
{
  NS_ASSERTION(mDeviceLibrary, "mDeviceLibrary is null, call init first.");
  SB_NOTIFY_LISTENERS_ASK_PERMISSION(OnBeforeAddAll(aMediaList,
                                                    &mShouldProcceed));
  if (mPerformAction) {
    return mDeviceLibrary->AddAll(aMediaList);
  } else {
    return NS_OK;
  }
}

/*
 * See sbIMediaList
 */
NS_IMETHODIMP 
sbDeviceLibrary::AddSome(nsISimpleEnumerator *aMediaItems)
{
  NS_ASSERTION(mDeviceLibrary, "mDeviceLibrary is null, call init first.");
  SB_NOTIFY_LISTENERS_ASK_PERMISSION(OnBeforeAddSome(aMediaItems,
                                                    &mShouldProcceed));
  if (mPerformAction) {
    return mDeviceLibrary->AddSome(aMediaItems);
  } else {
    return NS_OK;
  }
}

/*
 * See sbIMediaList
 */
NS_IMETHODIMP 
sbDeviceLibrary::Clear(void)
{
  NS_ASSERTION(mDeviceLibrary, "mDeviceLibrary is null, call init first.");
  SB_NOTIFY_LISTENERS_ASK_PERMISSION(OnBeforeClear(&mShouldProcceed));
  if (mPerformAction) {
    return mDeviceLibrary->Clear();
  } else {
    return NS_OK;
  }
}
