/* vim: set sw=2 :miv */
/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2009 POTI, Inc.
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
#include "sbTranscodeProgressListener.h"

// Songbird includes
#include <sbIJobProgress.h>
#include <sbIDeviceEvent.h>
#include <sbIMediacoreEventListener.h>
#include <sbIMediacoreEvent.h>
#include <sbIMediacoreError.h>
#include <sbStandardProperties.h>
#include <sbStatusPropertyValue.h>
#include <sbVariantUtils.h>

// Local includes
#include "sbDeviceStatusHelper.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(sbTranscodeProgressListener,
                              sbIJobProgressListener,
                              sbIMediacoreEventListener)

sbTranscodeProgressListener *
sbTranscodeProgressListener::New(sbBaseDevice * aDeviceBase,
                                 sbDeviceStatusHelper * aDeviceStatusHelper,
                                 sbBaseDevice::TransferRequest * aRequest,
                                 PRMonitor * aCompleteNotifyMonitor,
                                 StatusProperty const & aStatusProperty) {
  return new sbTranscodeProgressListener(aDeviceBase,
                                         aDeviceStatusHelper,
                                         aRequest,
                                         aCompleteNotifyMonitor,
                                         aStatusProperty);
}

sbTranscodeProgressListener::sbTranscodeProgressListener(
  sbBaseDevice * aDeviceBase,
  sbDeviceStatusHelper * aDeviceStatusHelper,
  sbBaseDevice::TransferRequest * aRequest,
  PRMonitor * aCompleteNotifyMonitor,
  StatusProperty const & aStatusProperty) :
    mBaseDevice(aDeviceBase),
    mStatus(aDeviceStatusHelper),
    mRequest(aRequest),
    mCompleteNotifyMonitor(aCompleteNotifyMonitor),
    mIsComplete(0),
    mTotal(0),
    mStatusProperty(aStatusProperty) {

  NS_ASSERTION(mBaseDevice,
               "sbTranscodeProgressListener mBaseDevice can't be null");
  NS_ASSERTION(mStatus,
               "sbTranscodeProgressListener mStatus can't be null");
  NS_ASSERTION(mRequest,
               "sbTranscodeProgressListener mRequest can't be null");
  NS_ASSERTION(mCompleteNotifyMonitor,
               "sbTranscodeProgressListener mCompleteNotifyMonitor "
               "can't be null");

  NS_IF_ADDREF(static_cast<sbIDevice*>(mBaseDevice));
}

sbTranscodeProgressListener::~sbTranscodeProgressListener() {
  sbIDevice * disambiguate = mBaseDevice;
  NS_IF_RELEASE(disambiguate);
}

nsresult
sbTranscodeProgressListener::Completed(sbIJobProgress * aJobProgress) {
  nsresult rv;

  sbStatusPropertyValue value;
  value.SetMode(sbStatusPropertyValue::eComplete);
  SetStatusProperty(value);
  // Indicate that the transcode operation is complete.  If a complete notify
  // monitor was provided, operate under the monitor and send completion
  // notification.
  if (mCompleteNotifyMonitor) {
    nsAutoMonitor monitor(mCompleteNotifyMonitor);
    PR_AtomicSet(&mIsComplete, 1);
    monitor.Notify();
  } else {
    PR_AtomicSet(&mIsComplete, 1);
  }

  // Disconnect us now that we're done.
  // This should be where we die as well, as this will be the last reference
  rv = aJobProgress->RemoveJobProgressListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbTranscodeProgressListener::SetProgress(sbIJobProgress * aJobProgress) {
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aJobProgress);

  if (!mTotal) {
    rv = aJobProgress->GetTotal(&mTotal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRUint32 progress;
  rv = aJobProgress->GetProgress(&progress);
  NS_ENSURE_SUCCESS(rv, rv);

  double const percentComplete = static_cast<double>(progress) /
                                 static_cast<double>(mTotal);
  mStatus->ItemProgress(percentComplete);

  sbStatusPropertyValue value;
  double const complete = percentComplete * 100.0;
  value.SetMode(sbStatusPropertyValue::eRipping);
  value.SetCurrent(complete);
  SetStatusProperty(value);

  return NS_OK;
}

NS_IMETHODIMP
sbTranscodeProgressListener::OnJobProgress(sbIJobProgress *aJobProgress)
{
  NS_ENSURE_ARG_POINTER(aJobProgress);

  PRUint16 status;
  nsresult rv = aJobProgress->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  switch (status) {
    case sbIJobProgress::STATUS_SUCCEEDED:
    case sbIJobProgress::STATUS_FAILED: {
      rv = Completed(aJobProgress);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    break;
    case sbIJobProgress::STATUS_RUNNING: {
      rv = SetProgress(aJobProgress);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    break;
    default: {
      NS_WARNING("Unexpected progress state in "
                 "sbTranscodeProgressListener::OnJobProgress");
    }
    break;
  }

  return NS_OK;
}

inline nsresult
sbTranscodeProgressListener::SetStatusProperty(
                                           sbStatusPropertyValue const & aValue)
{
  nsresult rv = mStatusProperty.SetValue(aValue.GetValue());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbTranscodeProgressListener::OnMediacoreEvent(sbIMediacoreEvent *aEvent)
{
  NS_ENSURE_ARG_POINTER(aEvent);

  nsresult rv;
  PRUint32 type;

  rv = aEvent->GetType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (type == sbIMediacoreEvent::ERROR_EVENT) {
    nsCOMPtr<sbIMediacoreError> error;
    rv = aEvent->GetError(getter_AddRefs(error));
    NS_ENSURE_SUCCESS(rv, rv);

    // Dispatch the device event
    mBaseDevice->CreateAndDispatchEvent(
          sbIDeviceEvent::EVENT_DEVICE_TRANSCODE_ERROR,
          sbNewVariant(NS_ISUPPORTS_CAST(sbIMediacoreError*, error)));
  }

  return NS_OK;
}

