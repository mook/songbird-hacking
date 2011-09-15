# There is a mishmash of make- and ini-style variable declarations in this file
# because we have to support older (1.1.x) style names for a bit longer;
# declaring them as ini-style, and then redefining them in make-style is
# messy, but makes it so we don't have to write 2 cases in the parser code;
# this can be removed when we're not using the buildbots to building the 
# 1.1-branch anymore.
SB_APPNAME=Songbird
SB_BUILD_ID=20110915050656
BuildNumber=2130
SB_BUILD_NUMBER=$(BuildNumber)
SB_MILESTONE=1.11.0a
SB_MILESTONE_WINDOWS=1.10.90.1
SB_BRANCHNAME=trunk
SB_PROFILE_VERSION=2
SB_MOZILLA_VERSION=6.0.1

# These are here to centralize our handling of these versions, since we
# use them; we make a slight distinction between JS-only and binary extensions
# for historical policy reasons; that's likely to change if/when we get
# nightly addon update/1st run bundles working
SB_BINARY_EXTENSION_MIN_VER=1.11.0a
SB_BINARY_EXTENSION_MAX_VER=1.11.0a
SB_JSONLY_EXTENSION_MIN_VER=1.11.0a
SB_JSONLY_EXTENSION_MAX_VER=1.11.0a

# Why is there no RepoRevisionClient, you ask? We have to treat client-pull
# revisions slightly differently, because we modify the repo itself whenever
# we do a build (bookend-build.py). The svn revision given below is done via
# keyword substitution, so in the common case (svn up to HEAD, in the daily
# case), it should be correct.
# BUT, it's provided merely as a convenience; the build system/buildbots rely on
# master-build-info.txt, which is sitting in the root of this repository...
# and if you want to be 100% sure/correct, so should you..
#
# RepoRevisionClient=$Rev$
RepoRevisionVendor=11396
RepoRevisionVendorBinaries=11904
