Ported.  What is left is the release: the SDK the tree pins is PROC_ABI 19, and
Sys::TermOpen -- which the second screen is -- landed in 20.  Move SDK_RELEASE
and SDK_VERSION in the Makefile and README when that ships, rebuild every
program, raise INDEX_VERSION and re-sign the repository.
