/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2010 POTI, Inc.
 * http://www.songbirdnest.com
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the ``GPL'').
 *
 * Software distributed under the License is distributed
 * on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied. See the GPL for the specific language
 * governing rights and limitations.
 *
 * You should have received a copy of the GPL along with this
 * program. If not, go to http://www.gnu.org/licenses/gpl.html
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *=END SONGBIRD GPL
 */

#include "sbTrackTypeImageLabelPropertyInfo.h"

#include <sbStringUtils.h>

#define SB_TRACKTYPE_PROPERTY_KEY "property.track_type.local"

sbTrackTypeImageLabelPropertyInfo::sbTrackTypeImageLabelPropertyInfo()
  : sbImageLabelLinkPropertyInfo()
{
  mType = NS_LITERAL_STRING("image");
  nsresult rv = sbImageLabelLinkPropertyInfo::Init();
  NS_ENSURE_SUCCESS(rv, /* void */);
}

/***** sbIClickablePropertyInfo */
NS_IMETHODIMP
sbTrackTypeImageLabelPropertyInfo::HitTest(const nsAString& aCurrentValue,
                                           const nsAString& aPart,
                                           PRUint32 aBoxWidth,
                                           PRUint32 aBoxHeight,
                                           PRUint32 aMouseX,
                                           PRUint32 aMouseY,
                                           PRBool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  return NS_OK;
}

/***** sbIClickablePropertyInfo */

NS_IMETHODIMP
sbTrackTypeImageLabelPropertyInfo::GetCellProperties(const nsAString& aValue,
                                                    nsAString& _retval)
{
  // If this is a track type column and the value is empty, only display label.
  if (aValue.IsEmpty()) {
    _retval.AssignLiteral("label");
    return NS_OK;
  }

  _retval.AssignLiteral("image label nolink");

  return NS_OK;
}

/***** sbIPropertyInfo */

NS_IMETHODIMP sbTrackTypeImageLabelPropertyInfo::Format(const nsAString & aValue,
                                                       nsAString & _retval)
{
  nsresult rv;

  // If this is a track type column and the value is empty, return local type
  // by default.
  if (aValue.IsEmpty()) {
    if (mTrackType.IsEmpty()) {
      mTrackType = SBLocalizedString(SB_TRACKTYPE_PROPERTY_KEY);
    }
    _retval.Assign(mTrackType);
    return NS_OK;
  }

  rv = sbImageLabelLinkPropertyInfo::Format(aValue, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}
