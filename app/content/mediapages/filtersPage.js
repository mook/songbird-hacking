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


if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;
if (typeof(Cu) == "undefined")
  var Cu = Components.utils;
if (typeof(Cr) == "undefined")
  var Cr = Components.results;


/**
 * Media Page Controller
 *
 * In order to display the contents of a library or list, pages
 * must provide a "window.mediaPage" object implementing
 * the Songbird sbIMediaPage interface. This interface allows
 * the rest of Songbird to talk to the page without knowledge 
 * of what the page looks like.
 *
 * In this particular page most functionality is simply 
 * delegated to the sb-playlist widget.
 */
window.mediaPage = {
    
    // The sbIMediaListView that this page is to display
  _mediaListView: null,
    
    // The sb-playlist XBL binding
  _playlist: null, 
    
  
  /** 
   * Gets the sbIMediaListView that this page is displaying
   */
  get mediaListView()  {
    return this._mediaListView;
  },
  
  
  /** 
   * Set the sbIMediaListView that this page is to display.
   * Called in the capturing phase of window load by the Songbird browser.
   * Note that to simplify page creation mediaListView may only be set once.
   */
  set mediaListView(value)  {
    
    if (!this._mediaListView) {
      this._mediaListView = value;
    } else {
      throw new Error("mediaListView may only be set once.  Please reload the page");
    }
  },
    
    
  /** 
   * Called when the page finishes loading.  
   * By this time window.mediaPage.mediaListView should have 
   * been externally set.  
   */
  onLoad:  function(e) {
    
    // Make sure we have the javascript modules we're going to use
    if (!window.SBProperties) 
      Cu.import("resource://app/jsmodules/sbProperties.jsm");
    if (!window.LibraryUtils) 
      Cu.import("resource://app/jsmodules/sbLibraryUtils.jsm");
    if (!window.kPlaylistCommands) 
      Cu.import("resource://app/jsmodules/kPlaylistCommands.jsm");
    
    if (!this._mediaListView) {
      Components.utils.reportError("playlistPage.xul did not receive  " + 
                                   "a mediaListView before the onload event!");
      return;
    }

    this._playlist = document.getElementById("playlist");

    // Configure the playlist filters based on the querystring
    this._updateFilterLists();
    this._generateFilters();

    // Get playlist commands (context menu, keyboard shortcuts, toolbar)
    // Note: playlist commands currently depend on the playlist widget.
    var mgr =
      Components.classes["@songbirdnest.com/Songbird/PlaylistCommandsManager;1"]
                .createInstance(Components.interfaces.sbIPlaylistCommandsManager);
    var cmds = mgr.request(kPlaylistCommands.MEDIAITEM_DEFAULT);

    // Set up the playlist widget
    this._playlist.bind(this._mediaListView, cmds);
  },
    
    
  /** 
   * Called as the window is about to unload
   */
  onUnload:  function(e) {
    for (var s = 0; s < this._splitters.length; s++) {
      this._splitters[s].removeEventListener("mousemove", this._onFilterSplitterMove, true);
      this._splitters[s].removeEventListener("mousedown", this._onFilterSplitterDown, true);
      this._splitters[s].removeEventListener("mouseup", this._onFilterSplitterUp, true);
    }
    
    if (this._playlist) {
      this._playlist.destroy();
      this._playlist = null;
    }
  },
    
  
  /** 
   * Show/highlight the MediaItem at the given MediaListView index.
   * Called by the Find Current Track button.
   */
  highlightItem: function(aIndex) {
    this._playlist.highlightItem(aIndex);
  },

  /** 
   * Called when something is dragged over the tabbrowser tab for this window
   */
  canDrop: function(aEvent, aSession) {
    return this._playlist.nsDragAndDropObserver.canDrop(aEvent, aSession);
  },

  /** 
   * Called when something is dropped on the tabbrowser tab for this window
   */
  onDrop: function(aEvent, aSession) {
    return this._playlist.
        _dropOnTree(this._playlist.mediaListView.length,
                    Ci.sbIMediaListViewTreeViewObserver.DROP_AFTER,
                    aSession);
  },

  /** 
   * Helper function to parse and unescape the query string. 
   * Returns a object mapping key->value
   */
  _parseQueryString: function() {
    var queryString = (location.search || "?").substr(1).split("&");
    var queryObject = {};
    var key, value;
    for each (var pair in queryString) {
      [key, value] = pair.split("=");
      queryObject[key] = unescape(value);
    }
    return queryObject;
  },

  /**
   * Configure the playlist filter lists
   */
  _updateFilterLists: function() {
    
    // This URL is registered as two different media pages. 
    // One with filters, one without. 
    // Use the querystring to determine what to show.
    var queryString = this._parseQueryString();
    
    var filters = this._mediaListView.cascadeFilterSet;
    
    // Set up standard filters if not already present.
    // Note that the first filter should be search.
    if (filters.length <= 1) {
      // Restore the last library filterset or set our default
      var filterSet = SBDataGetStringValue( "library.filterset" );
      if ( filterSet.length > 0 ) {
        filterSet = filterSet.split(";");
      }
      else {
        filterSet = [];
      }
      
      // if we have fewer than the default number of filters, append some extra
      var defaultFilterSet = [
        SBProperties.genre,
        SBProperties.artistName,
        SBProperties.albumName
      ];
      for (var i = filterSet.length; i < defaultFilterSet.length; i++) {
        filterSet.push(defaultFilterSet[i]);
      }
      
      for each (var filter in filterSet) {
        filters.appendFilter(filter);
      }
    }
  },

  /**
   * Generate the filter elements
   */
  _generateFilters: function() {
    // Hide the playlist splitter
    var psplitter = document.getElementById("sb-playlist-splitter");
    psplitter.setAttribute("hidden","true");

    // Get and empty the parent
    var parent = document.getElementById("sb-playlist-filters");
    while (parent.firstChild) {
      parent.removeChild(parent.firstChild);
    };

    // Create the filters based upon the cascadeFilterSet data.
    var cfs = this.mediaListView.cascadeFilterSet;
    var length = cfs.length;
    var filters = [];
    this._splitters = [];
    
    for (var i = 0; i < length; i++) {
      // If this is a search then keep going.
      if (cfs.isSearch(i)) {
        continue;
      }

      // And start creating the filter
      var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
      var filterlist = document.createElementNS(XUL_NS, "sb-filterlist");
      filterlist.setAttribute("enableColumnDrag", "false");
      filterlist.setAttribute("flex", "1");

      parent.appendChild(filterlist);
      
      filterlist.bind(this.mediaListView, i);
      filters.push(filterlist);

      var cfs = this.mediaListView.cascadeFilterSet;

      // If this filter has a search-term set, then pre-select it in the widget
      var filterValues = cfs.get(i).enumerate();
      while (filterValues.hasMoreElements()) {
        var filterValue = filterValues.getNext()
          .QueryInterface(Ci.nsISupportsString);
        // Now loop through our tree view and select this value
        for (var j = 0; j < filterlist.tree.view.rowCount; j++) {
          var col = filterlist.tree.columns.getColumnAt(0);
          if (filterlist.tree.view.getCellText(j, col) == filterValue)
            filterlist.tree.view.selection.select(j);
        }
      }

      if (i < length - 1) {
        var splitter = document.createElement("sb-smart-splitter");
        splitter.setAttribute("id", "filter_splitter" );
        splitter.setAttribute("stateid", "filter_splitter_" + cfs.getProperty(i) + this.mediaListView.mediaList.guid );
        splitter.setAttribute("state", "open");
        splitter.setAttribute("resizebefore", "closest");
        splitter.setAttribute("resizeafter", "closest");
        splitter.setAttribute("collapse", "before");

        var grippy = document.createElement( "grippy" );
        splitter.appendChild(grippy);
        // do not appendChild now, wait until all filterlists have been created (see below)
        this._splitters.push(splitter);
      }
    }
    
    // once the filterlists have been created, insert the splitters are their appropriate spot.
    // we do this now rather than during the filter creation loop because the splitters
    // need to be able to find their before/after targets on construction (so as to be able
    // to reload their positions correctly), so if we did do appendChild in the previous loop,
    // the after target would never be found, and the positions would be wrong
    for (var s=0;s<this._splitters.length;s++) {
      var splitter = this._splitters[s];
      parent.insertBefore(splitter, filters[s+1]);
    }

    // If we didn't actually add any filters then hide the filter box.
    if (!parent.firstChild) {
      parent.setAttribute("hidden", "true");
    } else {
      // Once we know we have at least one filter, show the playlist splitter.
      psplitter.removeAttribute("hidden");
    }
  }
} // End window.mediaPage 


